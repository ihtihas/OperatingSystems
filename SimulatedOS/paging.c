#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "simos.h"

// Memory definitions, including the memory itself and a page structure
// that maintains the informtion about each memory page
// config.sys input: pageSize, numFrames, OSpages
// ------------------------------------------------
// process page table definitions
// config.sys input: loadPpages, maxPpages

mType *Memory; // The physical memory, size = pageSize*numFrames

typedef unsigned ageType;
typedef struct
{
  int pid, page; // the frame is allocated to process pid for page page
  ageType age;
  char free, dirty, pinned; // in real systems, these are bits
  int next, prev;
} FrameStruct;

FrameStruct *memFrame;    // memFrame[numFrames]
int freeFhead, freeFtail; // the head and tail of free frame list

// define special values for page/frame number
#define nullIndex -1   // free frame list null pointer
#define nullPage -1    // page does not exist yet
#define diskPage -2    // page is on disk swap space
#define pendingPage -3 // page is pending till it is actually swapped                       \
                       // have to ensure: #memory-frames < address-space/2, (pageSize >= 2) \
                       //    becuase we use negative values with the frame number           \
                       // nullPage & diskPage are used in process page table

// define values for fields in FrameStruct
#define zeroAge 0x00000000
#define highestAge 0x80000000
#define dirtyFrame 1
#define cleanFrame 0
#define freeFrame 1
#define usedFrame 0
#define pinnedFrame 1
#define nopinFrame 0

// define shifts and masks for instruction and memory address
#define opcodeShift 24
#define operandMask 0x00ffffff

// shift address by pagenumShift bits to get the page number
unsigned pageoffsetMask;
int pagenumShift; // 2^pagenumShift = pageSize

//============================
// Our memory implementation is a mix of memory manager and physical memory.
// get_instr, put_instr, get_data, put_data are the physical memory operations
//   for instr, instr is fetched into registers: IRopcode and IRoperand
//   for data, data is fetched into registers: MBR (need to retain AC value)
// page table management is software implementation
//============================

//==========================================
// run time memory access operations, called by cpu.c
//==========================================

// define rwflag to indicate whehter the addr computation is for read or write
#define flagRead 1
#define flagWrite 2

// address calcuation are performed for the program in execution
// so, we can get address related infor from CPU registers
int calculate_memory_address(unsigned offset, int rwflag)
{
  // rwflag is used to differentiate the caller
  // different access violation decisions are made for reader/writer
  // if there is a page fault, need to set the page fault interrupt
  // also need to set the age and dirty fields accordingly
  // returns memory address or mPFault or mError
  //**Change**//
  int address, page, frame;
  page = (int)floor(offset / pageSize);

  if (page > maxPpages)
  {
    printf("Process %d accesses %d. Outside addr space!\n", CPU.Pid, page);
    return (mError);
  }

  frame = CPU.PTptr[page];

  if (frame == 0)
  {
    printf("Process %d accesses frame %d. OS address space!\n", CPU.Pid, frame);
    return (mError);
  }

  if (frame != nullPage && frame != diskPage)
  {
    offset = offset % pageSize;
    address = (offset & pageoffsetMask) | (frame << pagenumShift);

    memFrame[frame].age = highestAge;

    if (rwflag == actWrite)
    {
      memFrame[frame].dirty = dirtyFrame;
    }

    return address;
  }
  else
  {
    if (rwflag == actRead && frame == nullPage)
    {
      printf("Process %d accesses frame %d for read.Access Violation!\n", CPU.Pid);
      return mError;
    }
    else
    {
      printf("***Page Fault:pid/page= (%d,%d)\n", CPU.Pid, page);

      return mPFault;
    }
  }
}

int get_data(int offset)
{
  // call calculate_memory_address to get memory address
  // copy the memory content to MBR
  // return mNormal, mPFault or mError
  //**Change**//
  int ret = calculate_memory_address(offset, actRead);
  if (ret != mPFault && ret != mError)
  {
    int address = ret;
    CPU.MBR = Memory[address].mData;
    ret = mNormal;
  }
  return ret;
}

int put_data(int offset)
{
  // call calculate_memory_address to get memory address
  // copy MBR to memory
  // return mNormal, mPFault or mError
  //**Change**//
  int ret = calculate_memory_address(offset, actWrite);
  if (ret != mPFault & ret != mError)
  {
    int address = ret;
    Memory[address].mData = CPU.AC;
    ret = mNormal;
  }
  return ret;
}

int get_instruction(int offset)
{
  // call calculate_memory_address to get memory address
  // convert memory content to opcode and operand
  // return mNormal, mPFault or mError
  //**Change**//
  int ret = calculate_memory_address(offset, actRead);
  if (ret != mPFault & ret != mError)
  {
    int instr;
    int address = ret;
    instr = Memory[address].mInstr;
    CPU.IRopcode = instr >> opcodeShift;
    CPU.IRoperand = instr & operandMask;

    ret = mNormal;
  }
  return ret;
}

// these two direct_put functions are only called for loading idle process
// no specific protection check is done
void direct_put_instruction(int findex, int offset, int instr)
{
  int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  Memory[addr].mInstr = instr;
}

void direct_put_data(int findex, int offset, mdType data)
{
  int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  Memory[addr].mData = data;
}

//==========================================
// Memory and memory frame management
//==========================================

void dump_one_frame(int findex)
{
// dump the content of one memory frame
//**Change**
  printf("pid/page/age=%d,%d,%x, dir/free/pin=%d/%d/%d, next/prev=%d,%d\n", memFrame[findex].pid, memFrame[findex].page, memFrame[findex].age, memFrame[findex].dirty, memFrame[findex].free, memFrame[findex].pinned, memFrame[findex].next, memFrame[findex].prev);
  int start = (0&pageoffsetMask)|(findex<<pagenumShift);
  int end = ((pageSize-1)&pageoffsetMask)|(findex<<pagenumShift);
  printf("start-end:%d,%d: ", start,end);
  for (int offset = 0; offset < pageSize; offset++)
  {
    
    int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
    printf("%x ", Memory[addr]);
  }
  printf("\n");
}

void dump_memory()
{
  int i;

  printf("************ Dump the entire memory\n");
  for (i = 0; i < numFrames; i++)
    dump_one_frame(i);
}

// above: dump memory content, below: only dump frame infor

void dump_free_list()
{
  // dump the list of free memory frames
  //**Change
  printf("****List of free Frames****\n");
  if (freeFtail == nullIndex)
  {
    printf("No free frames available \n");
  }
  else
  {

    int frame = freeFhead;
    FrameStruct node = memFrame[frame];
    int i = 0;
    while (node.next != nullIndex)
    {
      if (++i % 8 == 0)
        printf("\n");
      printf("%d, ", frame);
      frame = node.next;
      node = memFrame[node.next];
    }
    printf("%d\n", freeFtail);
  }
}

void print_one_frameinfo(int indx)
{
  printf("pid/page/age=%d,%d,%x, ",
         memFrame[indx].pid, memFrame[indx].page, memFrame[indx].age);
  printf("dir/free/pin=%d/%d/%d, ",
         memFrame[indx].dirty, memFrame[indx].free, memFrame[indx].pinned);
  printf("next/prev=%d,%d\n",
         memFrame[indx].next, memFrame[indx].prev);
}

void dump_memoryframe_info()
{
  int i;

  printf("******************** Memory Frame Metadata\n");
  printf("Memory frame head/tail: %d/%d\n", freeFhead, freeFtail);
  for (i = OSpages; i < numFrames; i++)
  {
    printf("Frame %d: ", i);
    print_one_frameinfo(i);
  }
  dump_free_list();
}

void  update_frame_info (findex, pid, page)
int findex, pid, page;
{
  // update the metadata of a frame, need to consider different update scenarios
  // need this function also becuase loader also needs to update memFrame fields
  // while it is better to not to expose memFrame fields externally
	//**Change
  if (pid == nullPid && page == nullPage)
  { //case when process terminates
    memFrame[findex].age = zeroAge;
    memFrame[findex].free = freeFrame;
    memFrame[findex].dirty = cleanFrame;
  }
  else if (pid != nullPid)
  { //case when new frame is loaded to process.
    memFrame[findex].age = highestAge;
    memFrame[findex].free = usedFrame;
    memFrame[findex].dirty = cleanFrame;
  }
  memFrame[findex].pid = pid;
  memFrame[findex].page = page;
}

// should write dirty frames to disk and remove them from process page table
// but we delay updates till the actual swap (page_fault_handler)
// unless frames are from the terminated process (status = nullPage)
// so, the process can continue using the page, till actual swap
void addto_free_frame(int findex, int status)
{
  //**Change
  if (freeFtail == nullIndex)
  {
    freeFtail = findex;
    freeFhead = findex;
  }
  else // insert to tail
  {
    memFrame[freeFtail].next = findex;
    memFrame[findex].prev = freeFtail;
    memFrame[findex].next = nullIndex;
    freeFtail = findex;
  }

  if (status == nullPage)
  {
    update_frame_info(findex, nullPid, nullPage);
  }
  printf("Added free frame = %d\n",findex);
}

int select_agest_frame()
{
  // select a frame with the lowest age
  // if there are multiple frames with the same lowest age, then choose the one
  // that is not dirty
  //**Change
  int frame;
  int selected = 0;
  int minAge = highestAge;

  for (int i = OSpages; i < numFrames; i++)
  {
    if (memFrame[i].age == zeroAge)
    {
      if (selected == 0)
      {
        frame = i;
        selected = 1;
      }
      else
        addto_free_frame(i, pendingPage);
    }
    else
    {
      if (memFrame[i].age < minAge)
      {
        minAge = memFrame[i].age;
      }
    }
  }

  if (selected == 0)
  {
    int *minAgeList;
    minAgeList = (int *)(malloc(numFrames * sizeof(int)));
    int k = 0;
    for (int i = OSpages; i < numFrames; i++)
    {

      if (memFrame[i].age == minAge)
      {
        minAgeList[k] = i;
        k++;
        if (memFrame[i].dirty == cleanFrame)
        {
          if (selected == 0)
          {
            frame = i;
            selected = 1;
          }
          else
          {
            memFrame[i].age = zeroAge;
            addto_free_frame(i, pendingPage);
          }
        }
      }
    }

    if (selected == 0)
    {
      frame = minAgeList[0];
      selected = 1;
    }
  }
  return frame;
}

int get_free_frame()
{
  // get a free frame from the head of the free list
  // if there is no free frame, then get one frame with the lowest age
  // this func always returns a frame, either from free list or get one with lowest age
  //**Change
  int frame;
  if (freeFtail == nullIndex)
  {
    frame = select_agest_frame();
    printf("Select Agest frame = %d , age %d, dir %d\n", frame, memFrame[frame].age, memFrame[frame].dirty);
  }
  else
  {
    frame = freeFhead;
    if (freeFhead == freeFtail)
    {
      freeFtail = nullIndex;
    }
    freeFhead = memFrame[freeFhead].next;

    memFrame[freeFhead].prev = nullIndex;
    memFrame[frame].next = nullIndex;
    memFrame[frame].prev = nullIndex;
    if (memFrame[frame].age != 0)
    {
      printf("Frame is used recently. %d", frame);
      frame = get_free_frame();
    }
  }
  return frame;
}

void initialize_memory()
{
  int i;

  // create memory + create page frame array memFrame
  Memory = (mType *)malloc(numFrames * pageSize * sizeof(mType));
  
  for (int i = 0; i < numFrames * pageSize; i++)
  {
    Memory[i].mInstr = 0;
    Memory[i].mData = 0;
  }
  memFrame = (FrameStruct *)malloc(numFrames * sizeof(FrameStruct));

  // compute #bits for page offset, set pagenumShift and pageoffsetMask
  // *** ADD CODE
  pagenumShift = log2(pageSize);
  pageoffsetMask = log2(numFrames * pageSize);

  // initialize OS pages
  for (i=0; i<OSpages; i++)
  { memFrame[i].age = zeroAge;
    memFrame[i].dirty = cleanFrame;
    memFrame[i].free = usedFrame;
    memFrame[i].pinned = pinnedFrame;
    memFrame[i].pid = osPid;
  }
  // initilize the remaining pages, also put them in free list
  // *** ADD CODE
  freeFhead = nullIndex;
  freeFtail = nullIndex;
  for (i = OSpages; i < numFrames; i++)
  {
    memFrame[i].age = zeroAge;
    memFrame[i].dirty = cleanFrame;
    memFrame[i].free = freeFrame;
    memFrame[i].pinned = nopinFrame;
    memFrame[i].pid = nullPid;
    memFrame[i].page = nullPage;
    memFrame[i].next = nullIndex;
    memFrame[i].prev = nullIndex;

    if (freeFtail == nullIndex)
    {
      freeFtail = i;
      freeFhead = i;
    }
    else // insert to tail
    {
      memFrame[freeFtail].next = i;
      memFrame[i].prev = freeFtail;
      freeFtail = i;
    }
  }
  memFrame[freeFtail].next = nullIndex;
}

//==========================================
// process page table manamgement
//==========================================

void init_process_pagetable(int pid)
{
  int i;

  PCB[pid]->PTptr = (int *) malloc (addrSize*maxPpages);
  for (i=0; i<maxPpages; i++) PCB[pid]->PTptr[i] = nullPage;
}

// frame can be normal frame number or nullPage, diskPage
void update_process_pagetable (pid, page, frame)
int pid, page, frame;
{ 
  // update the page table entry for process pid to point to the frame
  // or point to disk or null
  //**Change
  printf("PT Update for (%d,%d) to %d\n", pid, page, frame);
  PCB[pid]->PTptr[page] = frame;
}

int free_process_memory(int pid)
{
  // free the memory frames for a terminated process
  // some frames may have already been freed, but still in process pagetable
  //**Change
  for (int i = 0; i < maxPpages; i++)
  {
    int frame = PCB[pid]->PTptr[i];

    if (frame != nullPage)
    {
      update_process_pagetable(pid, i, nullPage);
      if (frame != diskPage)
      {
        
        addto_free_frame(frame, nullPage);
      }
    }
  }
}

void dump_process_pagetable(int pid)
{
  // print page table entries of process pid
  //**Change
  printf("***********Page Table dump for process %d :\n", pid);
  for (int i = 0; i < maxPpages; i++)
  {
    int frame = PCB[pid]->PTptr[i];
    printf("%d, ", frame);
    if (((i+1) % 6) == 0)
    {
      printf("\n");
    }
  }
  printf("\n");
}

void dump_process_memory(int pid)
{
// print out the memory content for process pid
//**Change
  printf("**Memory Dump for the Process %d\n", pid);
  
  for (int i = 0; i < maxPpages; i++)
  {
    int frame = PCB[pid]->PTptr[i];

    printf("***P/F:%d,%d: ",i, frame);
    

    if (frame == nullPage)
    {
      printf("Nullpage\n");
    }
    else if (frame == diskPage)
    {
      dump_process_swap_page(pid, i);
    }
    else
    {
     
      dump_one_frame(frame);
    }
  }
}

//==========================================
// the major functions for paging, invoked externally
//==========================================

#define sendtoReady 1 // has to be the same as those in swap.c
#define notReady 0
#define actRead 0
#define actWrite 1

void page_fault_handler()
{
  // handle page fault
  // obtain a free frame or get a frame with the lowest age
  // if the frame is dirty, insert a write request to swapQ
  // insert a read request to swapQ to bring the new page to this frame
  // update the frame metadata and the page tables of the involved processes
  //**Change
  int page = (int)floor(CPU.PC / pageSize);
  if (CPU.PTptr[page] != nullPage && CPU.PTptr[page] != diskPage)
  {
    page = (int)floor(CPU.IRoperand / pageSize);
  }
  mType *buf;
  int frame = get_free_frame();
  printf("Got free frame = %d\n", frame);
   int oldPid = nullPid;
   int oldPage = nullPage;
  if (memFrame[frame].pid != nullPid)
  {
    oldPid = memFrame[frame].pid;
    oldPage= memFrame[frame].page;
    if (memFrame[frame].dirty != cleanFrame)
    {

      mType *buffer;
      buffer = (mType *)malloc(pageSize * sizeof(mType));
      for (int i = 0; i < pageSize; i++)
      {
        int addr = (i & pageoffsetMask) | (frame << pagenumShift);
        buffer[i] = Memory[addr];
      }
      insert_swapQ(oldPid, oldPage, buffer, actWrite, freeBuf);
    }
    if (PCB[oldPid] != NULL)
    {
      update_process_pagetable(oldPid, oldPage, diskPage);
    }
  }
  update_process_pagetable(CPU.Pid, page, frame);
  update_frame_info(frame, CPU.Pid, page);
  int address = (0 & pageoffsetMask) | (frame << pagenumShift);
  buf = &Memory[address];
  printf("swap_in: in= (%d,%d,%x), out= (%d,%d,%x)\n", CPU.Pid,page,buf,oldPid,oldPage,buf);
 
  printf("Page Fault Handler: pid/page=(%d,%d)\n", CPU.Pid, page);
  insert_endWait_process(CPU.Pid);
  insert_swapQ(CPU.Pid, page, buf, actRead, toReady);
}

// scan the memory and update the age field of each frame
void memory_agescan()
{
//**Change
  for (int i = OSpages; i < numFrames; i++)
  {
    memFrame[i].age = memFrame[i].age >> 1;
  }
}

void initialize_memory_manager()
{
  // initialize memory and add page scan event request
  //**Change
  initialize_memory();
  add_timer(periodAgeScan, osPid, actAgeInterrupt, periodAgeScan);
}

//**Change - New Function Added
void load_to_mem(int pid, int minPages)
{
  int page, frame;
  unsigned *buf;
  int finishAct;
  dump_process_pagetable(pid);
  for (page = 0; page < minPages; page++)
  {
    int frame = get_free_frame();
    dump_memoryframe_info();

    if (memFrame[frame].pid != nullPid)
    {
      int oldPid = memFrame[frame].pid;
      int oldPage = memFrame[frame].page;
      if (memFrame[frame].dirty != cleanFrame)
      {
        mType *buffer;
        buffer = (mType *)malloc(pageSize * sizeof(mType));
        for (int i = 0; i < pageSize; i++)
        {
          int addr = (i & pageoffsetMask) | (frame << pagenumShift);

          buffer[i] = Memory[addr];
        }

        insert_swapQ(oldPid, oldPage, buffer, actWrite, freeBuf);
      }
      if (PCB[oldPid] != NULL)
      {
        update_process_pagetable(oldPid, oldPage, diskPage);
      }
    }
    memFrame[frame].age = highestAge;
    int address = (0 & pageoffsetMask) | (frame << pagenumShift);
    buf = &Memory[address];
    update_process_pagetable(pid, page, frame);
    update_frame_info(frame, pid, page);
    if (page == minPages - 1)
    {
      finishAct = toReady;
    }
    else
    {
      finishAct = Nothing;
    }

    insert_swapQ(pid, page, buf, actRead, finishAct);
  }
  insert_endWait_process(pid);
}