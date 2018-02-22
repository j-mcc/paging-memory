/*
 * $Author: o1-mccune $
 * $Date: 2017/12/02 05:49:07 $
 * $Revision: 1.3 $
 * $Log: pcb.c,v $
 * Revision 1.3  2017/12/02 05:49:07  o1-mccune
 * removed comment
 *
 * Revision 1.2  2017/12/02 05:32:31  o1-mccune
 * fixed findingPageNumber
 *
 * Revision 1.1  2017/12/01 20:10:02  o1-mccune
 * Initial revision
 *
 */
#include "oss.h"
#include "pcb.h"
#include "paging.h"
#include "simulatedclock.h"
#include <stdio.h>
#include <stdlib.h>

void initPCB(pcb_t *pcb, pid_t pid, int pcbIndex, sim_clock_t *createdTime){
  pcb->pid = pid;
  pcb->priority = 0;
  pcb->state = CREATED;
  pcb->runMode = 5;
  
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++)
    pcb->aquiredResources[i] = 0;

  pcb->resourceRequest = -1;
  
  pcb->eventEnd.seconds = 0;
  pcb->eventEnd.nanoseconds = 0;

  pcb->totalWaitTime.seconds = 0;
  pcb->totalWaitTime.nanoseconds = 0;

  pcb->timeLastDispatch.seconds = 0;
  pcb->timeLastDispatch.nanoseconds = 0;
  
  pcb->timeCreated.seconds = createdTime->seconds;
  pcb->timeCreated.nanoseconds = createdTime->nanoseconds;
 
  pcb->timeInSystem.seconds = 0;
  pcb->timeInSystem.nanoseconds = 0;
  
  pcb->totalCPUTime.seconds = 0;
  pcb->totalCPUTime.nanoseconds = 0;
  
  pcb->lastBurstTime.seconds = 0;
  pcb->lastBurstTime.nanoseconds = 0;

  pcb->lastWaitTime.seconds = 0;
  pcb->lastWaitTime.nanoseconds = 0;


  //srand(time(0) + getpid());
  int numPages = (rand() % PROCESS_PAGE_MAX) + 1;
  pcb->pageTable.numPages = numPages;
  for(i = 0; i < numPages; i++){
    pcb->pageTable.page[i].processId = pcbIndex;
    pcb->pageTable.page[i].pageNum = i;
    pcb->pageTable.page[i].minAddress = i * PAGE_SIZE;
    pcb->pageTable.page[i].maxAddress = pcb->pageTable.page[i].minAddress + PAGE_SIZE -1;
  }
} 

int getReferenceLocationPageNumber(pcb_t *pcb, const int pcbIndex, const int address){
  int pageNum;
  for(pageNum = 1; pageNum <= pcb[pcbIndex].pageTable.numPages; pageNum++){
    if(address < (PAGE_SIZE * pageNum)) break;
  }
  return pageNum - 1;
}

page_t getPage(pcb_t *pcb, const int pageNum){
  return pcb->pageTable.page[pageNum];
}

void writePage(pcb_t *pcb, page_t page){
  pcb[page.processId].pageTable.page[page.pageNum] = page;
}

void addToTimeInSystem(pcb_t *pcb, sim_clock_t *time){
  sumSimClocks(&pcb->timeInSystem, time);  
}

void addToTotalCPUTime(pcb_t *pcb, sim_clock_t *time){
  sumSimClocks(&pcb->totalCPUTime, time);  
}

void setLastBurstTime(pcb_t *pcb, sim_clock_t *time){
  pcb->lastBurstTime.seconds = time->seconds;
  pcb->lastBurstTime.nanoseconds = time->nanoseconds;
}

void setLastWaitTime(pcb_t *pcb, sim_clock_t *time){
  if(pcb->timeLastDispatch.seconds == 0 && pcb->timeLastDispatch.nanoseconds == 0){
    pcb->lastWaitTime = findDifference(time, &pcb->timeCreated); 
  }
  else{
    pcb->lastWaitTime = findDifference(time, &pcb->timeLastDispatch);
  }
}

void setLastDispatchTime(pcb_t *pcb, sim_clock_t *time){
  setLastWaitTime(pcb, time);
  pcb->timeLastDispatch.seconds = time->seconds;
  pcb->timeLastDispatch.nanoseconds = time->nanoseconds;
}

pid_t getPID(pcb_t *pcb){
  return pcb->pid;
}

void incrementAquiredResource(int *aquiredResources, int resourceNumber){
  if(resourceNumber < NUM_RESOURCE_TYPES){
    aquiredResources[resourceNumber]++;
    //fprintf(stderr, "\t\tR[%d] was increased to %d\n", resourceNumber, aquiredResources[resourceNumber]);
  }
}

void decrementAquiredResource(int *aquiredResources, int resourceNumber){
  if(resourceNumber < NUM_RESOURCE_TYPES){
    if(aquiredResources[resourceNumber] == 0);
    else aquiredResources[resourceNumber]--;
    //fprintf(stderr, "\t\tR[%d] was decreased to %d\n", resourceNumber, aquiredResources[resourceNumber]);
  }
}

int findPCB(pcb_t *pcb, pid_t pid){
  if(pid == 0) return -2;
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(pcb[i].pid == pid) return i;
  }
  return -1;
}
