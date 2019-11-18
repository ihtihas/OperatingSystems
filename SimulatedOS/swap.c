#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>

#include "simos.h"

//======================================================================
// This module handles swap space management.
// It has the simulated disk and swamp manager.
// First part is for the simulated disk to read/write pages.
//======================================================================

#define swapFname "swap.disk"
#define itemPerLine 8
int diskfd, dumpfd;
int swapspaceSize;
int PswapSize;
int pagedataSize;

pthread_t swapThread;

sem_t swap_semaq;
sem_t swapq_mutex;
sem_t disk_mutex;

//===================================================
// This is the simulated disk, including disk read, write, dump.
// The unit is a page
//===================================================
// each process has a fix-sized swap space, its page count starts from 0
// first 2 processes: OS=0, idle=1, have no swap space
// OS frequently (like Linux) runs on physical memory address (fixed locations)
// virtual memory is too expensive and unnecessary for OS => no swap needed

int read_swap_page(int pid, int page, mType *buf)
{
  // reference the previous code for this part
  // but previous code was not fully completed
//**Change
  int location, ret, retsize, k;

  if (pid < 2 || pid > maxProcess)
  {
    printf("Error: Incorrect pid for disk read: %d\n", pid);
    return (-1);
  }

  location = (pid - 2) * PswapSize + page * pagedataSize;
  
  ret = lseek(diskfd, location, SEEK_SET);
  if (ret < 0)
    perror("Error lseek in read: \n");
  retsize = read(diskfd, buf, pagedataSize);

  
  if (retsize != pagedataSize)
  {
    printf("Error: Disk read returned incorrect size: %d\n", retsize);
    exit(-1);
  }
  usleep(diskRWtime);
}

int write_swap_page(int pid, int page, mType *buf)
{
  // reference the previous code for this part
  // but previous code was not fully completed
  //**Change
  sem_wait(&disk_mutex);
  int location, ret, retsize;

  if (pid < 2 || pid > maxProcess)
  {
    printf("Error: Incorrect pid for disk write: %d\n", pid);
    return (-1);
  }
  location = (pid - 2) * PswapSize + page * pagedataSize;
  ret = lseek(diskfd, location, SEEK_SET);
  if (ret < 0)
    perror("Error lseek in write: \n");
 
  
  retsize = write(diskfd, (char *)buf, pagedataSize);
  
  if (retsize != pagedataSize)
  {
    printf("Error: Disk Write returned incorrect size: %d\n", retsize);
    exit(-1);
  }
  usleep(diskRWtime);
  sem_post(&disk_mutex);
}

int dump_process_swap_page(int pid, int page)
{
  // reference the previous code for this part
  // but previous code was not fully completed
  //**Change
 sem_wait(&disk_mutex);
  int location, ret, retsize, k;
  mType *buf;
  buf = (mType *)malloc(pageSize * sizeof(mType));

  if (pid < 2 || pid > maxProcess)
  {
    printf("Error: Incorrect pid for disk dump: %d\n", pid);
    return (-1);
  }
 
  location = (pid - 2) * PswapSize + page * pagedataSize;
  ret = lseek(dumpfd, location, SEEK_SET);
  
  if (ret < 0)
    perror("Error lseek in dump: \n");
  retsize = read(dumpfd, buf, pagedataSize);
  if (retsize != pagedataSize)
  {
    printf("Error: Disk dump read incorrect size: %d\n", retsize);
    exit(-1);
  }
  
  printf("Content of process %d page %d:\n", pid, page);
  for (k = 0; k < pageSize; k++)
    printf("%x, ", buf[k]);
  printf("\n");
  sem_post(&disk_mutex);
}

void dump_process_swap(int pid)
{
  int j;

  printf("****** Dump swap pages for process %d\n", pid);
  
  for (j = 0; j < maxPpages; j++)
    dump_process_swap_page(pid, j);
  
}

// open the file with the swap space size, initialize content to 0
initialize_swap_space()
{
  int ret, i, j, k;
  mType *buf;
  buf = (mType *)malloc(pageSize * sizeof(mType));

  swapspaceSize = maxProcess * maxPpages * pageSize * dataSize;
  PswapSize = maxPpages * pageSize * dataSize;
  pagedataSize = pageSize * dataSize;

  diskfd = open(swapFname, O_RDWR | O_CREAT, 0600);
  dumpfd = diskfd;
  if (diskfd < 0)
  {
    perror("Error open: ");
    exit(-1);
  }
  ret = lseek(diskfd, swapspaceSize, SEEK_SET);
  if (ret < 0)
  {
    perror("Error lseek in open: ");
    exit(-1);
  }
  for (i = 2; i < maxProcess; i++)
    for (j = 0; j < maxPpages; j++)
    {
      for (k = 0; k < pageSize; k++)
      {
        buf[k].mInstr = 0;
        buf[k].mData = 0;
      }
      write_swap_page(i, j, buf);
    }
  // last parameter is the origin, offset from the origin, which can be:
  // SEEK_SET: 0, SEEK_CUR: from current position, SEEK_END: from eof
}

//===================================================
// Here is the swap space manager.
//===================================================
// When a process address to be read/written is not in the memory,
// meory raises a page fault and process it (in kernel mode).
// We implement this by cheating a bit.
// We do not perform context switch right away and switch to OS.
// We simply let OS do the processing.
// OS decides whether there is free memory frame, if so, use one.
// If no free memory, then call select_aged_page to free up memory.
// In either case, proceed to insert the page fault req to swap queue
// to let the swap manager bring in the page
//===================================================

typedef struct SwapQnodeStruct
{
  int pid, page, act, finishact;
  mType *buf;
  struct SwapQnodeStruct *next;
} SwapQnode;
// pidin, pagein, inbuf: for the page with PF, needs to be brought in
// pidout, pageout, outbuf: for the page to be swapped out
// if there is no page to be swapped out (not dirty), then pidout = nullPid
// inbuf and outbuf are the actual memory page content

SwapQnode *swapQhead = NULL;
SwapQnode *swapQtail = NULL;

void print_one_swapnode(SwapQnode *node)
{
  printf("pid,page=(%d,%d), act,ready=(%d, %d), buf=%x\n",
         node->pid, node->page, node->act, node->finishact, node->buf);
}

void dump_swapQ()
{
  // dump all the nodes in the swapQ
//**Change
  SwapQnode *node;

  printf ("******************** Swap Queue Dump\n");
  node = swapQhead;
  while (node != NULL)
  {  print_one_swapnode(node);
      node = node->next;
  }
  
}

// act can be actRead or actWrite
// finishact indicates what to do after read/write swap disk is done, it can be:
// toReady (send pid back to ready queue), freeBuf: free buf, Both, Nothing
void insert_swapQ(pid, page, buf, act, finishact) int pid, page, act, finishact;
mType *buf;
{
  //**Change
  SwapQnode *node;
  sem_wait(&swapq_mutex);

  if(Debug) printf("Insert SwapQ pid,page=(%d,%d),  act,fact=(%d,%d), buf=%x\n",pid,page,act,finishact,buf);

  node = (SwapQnode *)malloc(sizeof(SwapQnode));

  node->buf = (mType *)malloc(pagedataSize * sizeof(mType));

  node->pid = pid;

  node->page = page;

  node->buf = buf;

  node->act = act;

  node->finishact = finishact;

  node->next = NULL;
  

  if (swapQtail == NULL)
  {
    swapQhead = node;
    swapQtail = node;
  }
  else
  {

    swapQtail->next = node;
    swapQtail = node;
  }

  sem_post(&swapq_mutex);
  sem_post(&swap_semaq);
  
}

//**Change
void handle_one_swapReq()
{



  SwapQnode *node;
  sem_wait(&swap_semaq);
  sem_wait(&swapq_mutex);
  if (Debug) dump_swapQ ();
  if (swapQhead == NULL)
  {
    printf("No swap requests in Swap Queue!\n");
  }
  else
  {
    node = swapQhead;
    if (node->act == actWrite)
    {
      write_swap_page(node->pid, node->page, node->buf);
    }
    else if (node->act == actRead)
    {
    
      read_swap_page(node->pid, node->page, node->buf);
      int frame = PCB[node->pid]->PTptr[node->page];
    }
    if(node->finishact == toReady || node->finishact == Both){
      
      set_interrupt(endWaitInterrupt);
    }
    if(node->finishact == freeBuf || node->finishact == Both){
      free(node->buf);
    }
    swapQhead = node->next;
    if (swapQhead == NULL)
      swapQtail = NULL;
  }
  sem_post(&swapq_mutex);
}

void *process_swapQ()
{
  // called as the entry function for the swap thread
  while (systemActive)
    handle_one_swapReq();
  printf("Swap Manager Loop has ended.\n");
}

void start_swap_manager()
{
  int ret;
  // initialize semaphores
  sem_init(&swap_semaq, 0, 0);
  sem_init(&swapq_mutex, 0, 1);
  sem_init(&disk_mutex, 0, 1);
  // initialize_swap_space ();
  initialize_swap_space();
  // create swap thread
  ret = pthread_create(&swapThread, NULL, process_swapQ, NULL);
  if (ret < 0)
    printf("Swap Thread creation problem\n");
  else
    printf("Swap thread has been created successsfully\n");
}

void end_swap_manager()
{
  // terminate the swap thread
  int ret;

  ret = pthread_join(swapThread, NULL);
  printf("SwapManager thread has terminated %d\n", ret);
}
