#include <stdio.h>
#include <stdlib.h>
#include "simos.h"

// need to be consistent with paging.c: mType and constant definitions
#define opcodeShift 24
#define operandMask 0x00ffffff
#define diskPage -2

FILE *progFd;


//==========================================
// load program into memory and build the process, called by process.c
// a specific pid is needed for loading, since registers are not for this pid
//==========================================

// may return progNormal or progError (the latter, if the program is incorrect)
int load_instruction(mType *buf, int page, int offset)
{
//**Change Start**//
  int opcode, operand;
  int ret = fscanf(progFd, "%d %d\n", &opcode, &operand);

  if (ret < 2) // did not get all three inputs
  {
    printf("Submission failure: missing %d program parameters!\n", 2 - ret);
    return progError;
  }
  opcode = opcode << opcodeShift;
  operand = operand & operandMask;
  printf("Load Instruction (%x, %d) into M(%d,%d)\n", opcode, operand, page, offset);
  buf[offset].mInstr = opcode | operand;

  return progNormal;
  //**CHange End**//
}

int load_data(mType *buf, int page, int offset)
{
  // load data to buffer (same as load instruction, but use the mData field
  //**Change Start**//
  float data;
  int ret = fscanf(progFd, "%f\n", &data);
  buf[offset].mData = data;
  printf("Load Data: %f into M(%d,%d)\n", data, page, offset);
  return progNormal;
  //**Change End**//
}

// load program to swap space, returns the #pages loaded
int load_process_to_swap(int pid, char *fname)
{
  // read from program file "fname" and call load_instruction & load_data
  // to load the program into the buffer, write the program into
  // swap space by inserting it to swapQ
  // update the process page table to indicate that the page is not empty
  // and it is on disk (= diskPage)
  //**Change Start**//
  mType *buffer;
  progFd = fopen(fname, "r");
  if (progFd == NULL)
  {
    printf("Submission Error: Incorrect program name: %s!\n", fname);
    return;
  }

  int numInstr, numData, memSize;
  int ret;

  ret = fscanf(progFd, "%d %d %d\n", &memSize, &numInstr, &numData);

  if (ret < 3) // did not get all three inputs
  {
    printf("Submission failure: missing %d program parameters!\n", 3 - ret);
    return progError;
  }

  if (memSize > (maxPpages * pageSize * sizeof(mType)))
  {
    printf("Invalid memory size %d for process %d\n", memSize, pid);
    return (mError);
  }
  int pages = (numInstr+numData)/pageSize;

  buffer = (mType *)malloc(pageSize * sizeof(mType));
  bzero(buffer, sizeof(buffer));
  int page = 0;
  int offset = 0;
  for (int i = 0; i < numInstr; i++)
  {

    ret = load_instruction(buffer, page, offset);

    if (ret == progError)
    {
      return ret;
    }
    offset++;
    if (offset % pageSize == 0)
    {
      update_process_pagetable(pid, page, diskPage);
      insert_swapQ(pid, page, buffer, actWrite, freeBuf);

      buffer = (mType *)malloc(pageSize * sizeof(mType));
      bzero(buffer, sizeof(buffer));
      page++;
      offset = 0;
    }
  }

  for (int i = numInstr; i < numInstr + numData; i++)
  {

    load_data(buffer, page, offset);

    offset++;
    if (offset % pageSize == 0)
    {
      update_process_pagetable(pid, page, diskPage);
      insert_swapQ(pid, page, buffer, actWrite, freeBuf);

      buffer = (mType *)malloc(pageSize * sizeof(mType));
      bzero(buffer, sizeof(buffer));
      page++;
      offset = 0;
    }
  }

  if ((page-1) != pages)
  {
    update_process_pagetable(pid, page, diskPage);
    insert_swapQ(pid, page, buffer, actWrite, freeBuf);
  }
  return pages+1;
  //**Change End**//
}

int load_pages_to_memory(int pid, int numpage)
{
  // call insert_swapQ to load the pages of process pid to memory
  // #pages to load = min (loadPpages, numpage = #pages loaded to swap for pid)
  // ask swap.c to place the process to ready queue only after the last load
  // do not forget to update the page table of the process
  // this function has some similarity with page fault handler
  //**Change Start**//
  int minPages = (loadPpages < numpage) ? loadPpages : numpage;
  load_to_mem(pid, minPages);
  //**Change End**//
}

int load_process(int pid, char *fname)
{
  int ret;
  init_process_pagetable(pid);
  ret = load_process_to_swap(pid, fname); // return #pages loaded
  if (ret != progError)
  {
    printf("Load %d pages for process %d\n", ret, pid);
    load_pages_to_memory(pid, ret);
  }
  return (ret);
}

// load idle process, idle process uses OS memory
// We give the last page of OS memory to the idle process
#define OPifgo 5 // has to be consistent with cpu.c
void load_idle_process()
{
  int page, frame;
  int instr, opcode, operand, data;

  init_process_pagetable(idlePid);
  page = 0;
  frame = OSpages - 1;
  update_process_pagetable(idlePid, page, frame);
  update_frame_info(frame, idlePid, page);

  // load 1 ifgo instructions (2 words) and 1 data for the idle process
  opcode = OPifgo;
  operand = 0;
  instr = (opcode << opcodeShift) | operand;
  direct_put_instruction(frame, 0, instr); // 0,1,2 are offset
  direct_put_instruction(frame, 1, instr);
  mdType data_load = 1.0;
  direct_put_data(frame, 2, data_load);
}
