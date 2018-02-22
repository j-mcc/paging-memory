#ifndef PAGING_H
#define PAGING_H

#include <limits.h>
#include "simulatedclock.h"
#include "bitarray.h"

#define TOTAL_FRAMES 256
#define PROCESS_PAGE_MAX 32
#define PAGE_SIZE 1024
#define THRESHOLD .1

typedef struct device_queue_item_t{
  unsigned int processId;
  unsigned int pageNum;
  struct device_queue_item_t *next;
} device_queue_item_t;

typedef struct {
  sim_clock_t timer;
  device_queue_item_t *head;
} device_queue_t;

typedef struct {
  unsigned int processId;
  unsigned int pageNum;
  unsigned int minAddress;
  unsigned int maxAddress;
} page_t;
  
typedef struct {
  page_t page[PROCESS_PAGE_MAX];
  unsigned char numPages;
} page_table_t;

typedef struct {
  page_t page;
} frame_t;

typedef struct {
  frame_t frames[TOTAL_FRAMES];
  bitarray_t availableFrameBits;
  bitarray_t dirtyBits;
  bitarray_t secondChanceBits;
} frame_table_t;

int initFrameTable(frame_table_t *frameTable);

int initDeviceQueue(device_queue_t *deviceQueue);

int removeHeadDeviceQueue(device_queue_t *deviceQueue, sim_clock_t *simClock);

int insertDeviceQueue(device_queue_t *deviceQueue, const int pcbIndex, const int pageNum, sim_clock_t *simClock);

page_t loadPage(frame_table_t *frameTable, page_t newPage, int *dirty);

int pageIsResident(frame_table_t *frameTable, const int pcbIndex, const int pageNum);

void purgeFrameTable(frame_table_t *frameTable, const int pcbIndex);

int availableFramesBelowThreshold(bitarray_t *availableFrames);

int deviceFinished(device_queue_t *deviceQueue, sim_clock_t *simClock);

#endif
