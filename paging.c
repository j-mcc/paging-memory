/*
 * $Author: o1-mccune $
 * $Date: 2017/12/02 05:43:43 $
 * $Revision: 1.2 $
 * $Log: paging.c,v $
 * Revision 1.2  2017/12/02 05:43:43  o1-mccune
 * final revision.
 *
 * Revision 1.1  2017/12/01 20:09:30  o1-mccune
 * Initial revision
 *
 */

#include "paging.h"
#include <stdio.h>
#include <stdlib.h>
#include "pcb.h"
#include "bitarray.h"
#include "simulatedclock.h"
#include "math.h"


static sim_clock_t  pagingDelay;

int initDeviceQueue(device_queue_t *deviceQueue){
  deviceQueue->timer.seconds = 0;
  deviceQueue->timer.nanoseconds = 0;
  deviceQueue->head = NULL;

  pagingDelay.seconds = 0;
  pagingDelay.nanoseconds = 15000000;
}

int deviceFinished(device_queue_t *deviceQueue, sim_clock_t *simClock){
  if(compareSimClocks(simClock, &deviceQueue->timer) == 1 && (deviceQueue->timer.nanoseconds + deviceQueue->timer.seconds) != 0) return 1;
  return 0;
}

int insertDeviceQueue(device_queue_t *deviceQueue, const int pcbIndex, const int pageNum, sim_clock_t *simClock){
  //create item
  device_queue_item_t *item = malloc(sizeof(device_queue_item_t));
  if(item){
    item->processId = pcbIndex;
    item->pageNum = pageNum;
    item->next = NULL;
  }
  
  //insert item
  if(deviceQueue){
    if(deviceQueue->head){  //put after last entry
      device_queue_item_t *iterator = deviceQueue->head;
      while(iterator->next){
        iterator = iterator->next;
      }
      iterator->next = item;
    }
    else{  //put at head
      deviceQueue->head = item;
      //start timer
      sumSimClocks(&deviceQueue->timer, simClock);
      sumSimClocks(&deviceQueue->timer, &pagingDelay);
    }
  }
  else{
    if(item) free(item);
    fprintf(stderr, "DEVICE QUEUE IS NULL\n");
    return 0;
  }
  return 1;
}

int removeHeadDeviceQueue(device_queue_t *deviceQueue, sim_clock_t *simClock){
  if(deviceQueue){
    if(deviceQueue->head){
      deviceQueue->head = deviceQueue->head->next;
      if(deviceQueue->head){
        sumSimClocks(&deviceQueue->timer, simClock);
        sumSimClocks(&deviceQueue->timer, &pagingDelay);
      }
      else{
        deviceQueue->timer.seconds = 0;
        deviceQueue->timer.nanoseconds = 0;
      }
    }
  }
}

int initFrameTable(frame_table_t *frameTable){
  initBitVector(&frameTable->availableFrameBits, TOTAL_FRAMES, MAX_VALUE);
  initBitVector(&frameTable->dirtyBits, TOTAL_FRAMES, MIN_VALUE);
  initBitVector(&frameTable->secondChanceBits, TOTAL_FRAMES, MIN_VALUE);
  return 1;
}

page_t loadPage(frame_table_t *frameTable, page_t newPage, int *dirty){
  
  page_t pageReplaced;

  //if # free frames < 10% of total
  if(availableFramesBelowThreshold(&frameTable->availableFrameBits)){
    //sweep frame table and turn off secondChanceBits until a reclaimable frame is found
    int i, j;
    for(i = 0; i < 2; i++){
      for(j = 0; j < TOTAL_FRAMES; j++){
        if(getBit(&frameTable->secondChanceBits, j)) clearBit(&frameTable->secondChanceBits, j);
        else{
          i++;
          break; 
        }
      }
    }
    if(j != TOTAL_FRAMES){  //frame to replace has been found
      if(getBit(&frameTable->dirtyBits, j)){  //dirty bit is set, need to write this page back to disk
        *dirty = 1;
        pageReplaced = frameTable->frames[j].page;
      }
      //bring in new page
      frameTable->frames[j].page = newPage;
      clearBit(&frameTable->dirtyBits, j);
      setBit(&frameTable->secondChanceBits, j);
      clearBit(&frameTable->availableFrameBits, j);
    }
    else fprintf(stderr, "Error finding available frame\n");
  }
  else{  //find next free frame and insert page
    int frameNumber = indexOfNextSetBit(&frameTable->availableFrameBits);
    frameTable->frames[frameNumber].page = newPage;
    clearBit(&frameTable->dirtyBits, frameNumber);
    setBit(&frameTable->secondChanceBits, frameNumber);
    clearBit(&frameTable->availableFrameBits, frameNumber);
  }

  return pageReplaced;
}

int pageIsResident(frame_table_t *frameTable, const int pcbIndex, const int pageNum){
  int i;
  for(i = 0; i < TOTAL_FRAMES; i++){
    if(frameTable->frames[i].page.processId == pcbIndex && frameTable->frames[i].page.pageNum == pageNum) return 1;
  }
  return 0;
}

void purgeFrameTable(frame_table_t *frameTable, const int pcbIndex){
  int i;
  for(i = 0; i < TOTAL_FRAMES; i++){
    if(frameTable->frames[i].page.processId == pcbIndex){
      setBit(&frameTable->availableFrameBits, i);
      clearBit(&frameTable->dirtyBits, i);
      clearBit(&frameTable->secondChanceBits, i);
    }
  }
}

int availableFramesBelowThreshold(bitarray_t *availableFrames){
  if(getNumberSetBits(availableFrames) < ceil(THRESHOLD * TOTAL_FRAMES)) return 1;
  return 0;
}
