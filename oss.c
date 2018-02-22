
 /*
 * $Author: o1-mccune $
 * $Date: 2017/12/02 05:48:51 $
 * $Revision: 1.3 $
 * $Log: oss.c,v $
 * Revision 1.3  2017/12/02 05:48:51  o1-mccune
 * fixed output
 *
 * Revision 1.2  2017/12/02 05:42:29  o1-mccune
 * fixed issue with sequence of handling messages. Added required output.
 *
 * Revision 1.1  2017/12/01 20:08:42  o1-mccune
 * Initial revision
 *
 */

#include "oss.h"
#include "paging.h"
#include "bitarray.h"
#include "pcb.h"
#include "simulatedclock.h"
#include "messagequeue.h"
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>


static int logEntries = 0;
static int logEntryCutoff = 100000;
static unsigned int maxProcessTime = 20;
static char defaultLogFilePath[] = "logfile.txt";
static char *logFilePath = NULL;
static FILE *logFile;

/*--System Report Variables--*/
static int numAllocations = 0;
static int finished[PCB_TABLE_SIZE];
//static int ranDeadlockDetection = 0;
//static int numDeadlocks = 0;
//static int newDeadlock = 0;
//static int numProcessesSegmentFault = 0;
/*---------------------------*/

/*--Shared Memory Variables--*/
static pcb_t *pcb;
static frame_table_t frameTable;
static sim_clock_t *simClock;
static message_t messageBuffer;
static key_t messageQueueKey;
static key_t clockSharedMemoryKey;
static key_t pcbSharedMemoryKey;
static int messageQueueId;
static int pcbSharedMemoryId;
static int clockSharedMemoryId;
/*---------------------------*/


static bitarray_t pcbBitVector;
//static bitarray_t dirtyBitVector;
//static bitarray_t secondChanceBitVector;
static device_queue_t deviceQueue;

static pid_t *children;
static int childCounter = 0;
static int terminatedIndex;

static unsigned int verbose = 0; //By default, verbose logging is off

/*-------------------------------------------*/

static void printOptions(){
  fprintf(stderr, "OSS:  Command Help\n");
  fprintf(stderr, "\tOSS:  '-h': Prints Command Usage\n");
  fprintf(stderr, "\tOSS:  Optional '-l': Filename of log file. Default is logfile.txt\n");
  fprintf(stderr, "\tOSS:  Optional '-t': Input number of seconds before the main process terminates. Default is 20 seconds.\n");
  fprintf(stderr, "\tOSS:  Optional '-v': If option -v is specified, verbose(increased) logging will be performed.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  while ((c = getopt (argc, argv, "ht:l:v")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 't':
        maxProcessTime = atoi(optarg);
        break;
      case 'l':
	logFilePath = malloc(sizeof(char) * (strlen(optarg) + 1));
        memcpy(logFilePath, optarg, strlen(optarg));
        logFilePath[strlen(optarg)] = '\0';
        break;
      case 'v':
        verbose = 1;
        break;
      case '?':
        if(optopt == 't'){
          maxProcessTime = 20;
	  break;
	}
	else if(isprint (optopt))
          fprintf(stderr, "OSS: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "OSS: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  if(!logFilePath){
    logFilePath = malloc(sizeof(char) * strlen(defaultLogFilePath) + 1);
    memcpy(logFilePath, defaultLogFilePath, strlen(defaultLogFilePath));
    logFilePath[strlen(defaultLogFilePath)] = '\0';
  }
  return 0;
}

static int initMessageQueue(){
  if((messageQueueKey = ftok("./oss", 10)) == -1) return -1;
  if((messageQueueId = getMessageQueue(messageQueueKey)) == -1){
    perror("OSS: Failed to get message queue");
    return -1;
  }
  return 0;
}
/*
static int initSemaphoreSharedMemory(){
  if((semaphoreSharedMemoryKey = ftok("./oss", 1)) == -1) return -1;
  if((semaphoreSharedMemoryId = shmget(semaphoreSharedMemoryKey, sizeof(sem_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for semaphore");
    return -1;
  }
  return 0;
}
*/
static int initPCBSharedMemory(){
  if((pcbSharedMemoryKey = ftok("./oss", 2)) == -1) return -1;
  if((pcbSharedMemoryId = shmget(pcbSharedMemoryKey, sizeof(pcb_t) * PCB_TABLE_SIZE, IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for PCB");
    return -1;
  }
  return 0;
}

static int initClockSharedMemory(){
  if((clockSharedMemoryKey = ftok("./oss", 3)) == -1) return -1;
  if((clockSharedMemoryId = shmget(clockSharedMemoryKey, sizeof(sim_clock_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for clock");
    return -1;
  }
  return 0;
}

static int removePCBSharedMemory(){
  if(shmctl(pcbSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove PCB shared memory");
    return -1;
  }
  return 0;
}

static int removeClockSharedMemory(){
  if(shmctl(clockSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove clock shared memory");
    return -1;
  }
  return 0;
}

static int detachClockSharedMemory(){
  return shmdt(simClock);
}

static int detachPCBSharedMemory(){
  return shmdt(pcb);
}

static int attachPCBSharedMemory(){
  if((pcb = shmat(pcbSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachClockSharedMemory(){
  if((simClock = shmat(clockSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static void cleanUp(int signal){
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(signal == 2) fprintf(stderr, "Parent sent SIGINT to Child %d\n", pcb[i].pid);
    else if(signal == 14)fprintf(stderr, "Parent sent SIGALRM to Child %d\n", pcb[i].pid);
    kill(pcb[i].pid, signal);
    waitpid(-1, NULL, 0);
  }
  
  if(removeMessageQueue(messageQueueId) == -1) perror("OSS: Failed to remove message queue");
  if(detachPCBSharedMemory() == -1) perror("OSS: Failed to detach PCB memory");
  if(removePCBSharedMemory() == -1) perror("OSS: Failed to remove PCB memory");
  if(detachClockSharedMemory() == -1) perror("OSS: Failed to detach clock memory");
  if(removeClockSharedMemory() == -1) perror("OSS: Failed to remove clock memory");
  if(children) free(children);
  if(logFile) fclose(logFile);
  freeBitArray(&pcbBitVector);
  freeBitArray(&frameTable.availableFrameBits);
  freeBitArray(&frameTable.dirtyBits);
  freeBitArray(&frameTable.secondChanceBits);
  fprintf(stderr, "OSS: Total children created :: %d\n", childCounter); 
  //fprintf(stderr, "OSS: Total number of deadlock checks :: %d\n", ranDeadlockDetection);
  //fprintf(stderr, "OSS: Number of processes killed naturally :: %d\n", childCounter - numProcessesTerminatedByDeadlockResolver);
  //fprintf(stderr, "OSS: Number of processes killed to fix deadlock :: %d\n", numProcessesTerminatedByDeadlockResolver);
  //fprintf(stderr, "OSS: Average number of processes killed to resolve deadlock :: %f\n", ((numProcessesTerminatedByDeadlockResolver * 1.)/numDeadlocks));
  //fprintf(stderr, "OSS: Number of resource allocations :: %d\n", numAllocations);
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(signal);
}

static int initAlarmWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL));
}

static int initInterruptWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGINT, &action, NULL));
}

/*
 *  Transform an integer into a string
 */
static char *itoa(int num){
  char *asString = malloc(sizeof(char)*16);
  snprintf(asString, sizeof(char)*16, "%d", num);
  return asString;
}

static void printAllocationTable(){
  int i, j;
  for(i = 0; i < (TOTAL_FRAMES / 8); i++){
    for(j = 0; j < 8; j++){
      int index = (i * 8) + j;
      if(getBit(&frameTable.availableFrameBits, index)) fprintf(stderr, "F[%3d]->(     .     ) \t", index);
      else fprintf(stderr, "F[%3d]->(C[%2d]:P[%2d]) \t", index, frameTable.frames[index].page.processId, frameTable.frames[index].page.pageNum);
    }
    fprintf(stderr, "\n");
  }

  for(i = 0; i < (TOTAL_FRAMES / 8); i++){
    for(j = 0; j < 8; j++){
      int index = (i * 8) + j;
      if(getBit(&frameTable.availableFrameBits, index)) fprintf(logFile, "F[%3d]->(     .     ) \t", index);
      else fprintf(logFile, "F[%3d]->(C[%2d]:P[%2d]) \t", index, frameTable.frames[index].page.processId, frameTable.frames[index].page.pageNum);
    }
    fprintf(logFile, "\n");
  }
  fprintf(logFile, "\n");
  fprintf(logFile, "\n");
}

static void logRequest(int childNum, int page){
  fprintf(stderr, "OSS: Received REQUEST from C[%d] for P[%d]", childNum, page);
  fprintf(stderr, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
  /*if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: Received REQUEST from C[%d] for ", childNum);
      fprintf(logFile, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
    }
  }*/
}


static void logTerminate(int childNum, int numReferences){
  sim_clock_t average = pcb[childNum].timeInSystem;
  findAverage(&average, numReferences);

  fprintf(stderr, "OSS: Received TERMINATE from C[%d] @ time [%d:%d]. Average Access Time: %d:%d\n", childNum, simClock->seconds, simClock->nanoseconds, average.seconds, average.nanoseconds); 
  //if(logEntries++ < logEntryCutoff){
    //if(verbose){ 
      fprintf(logFile, "OSS: Received TERMINATE from C[%d] @ time [%d:%d]. Average Access Time: %d:%d\n", childNum, simClock->seconds, simClock->nanoseconds, average.seconds, average.nanoseconds); 
    //}
  //}
}


int main(int argc, char **argv){
  parseOptions(argc, argv);

  children = malloc(sizeof(pid_t) * PCB_TABLE_SIZE);
  logFile = fopen(logFilePath, "w");
  

  if(initAlarmWatcher() == -1) perror("OSS: Failed to init SIGALRM watcher");
  if(initInterruptWatcher() == -1) perror("OSS: Failed to init SIGINT watcher");
  
  if(initMessageQueue() == -1) perror("OSS: Failed to init message queue");
  if(initPCBSharedMemory() == -1) perror("OSS: Failed to init PCB memory");
  if(attachPCBSharedMemory() == -1) perror("OSS: Failed to attach PCB memory");
  if(initClockSharedMemory() == -1) perror("OSS: Failed to init clock memory");
  if(attachClockSharedMemory() == -1) perror("OSS: Failed to attach clock memory");
  if(initBitVector(&pcbBitVector, PCB_TABLE_SIZE, MIN_VALUE) == -1) fprintf(stderr, "OSS: Failed to initialize pcb bit vector\n");
  initFrameTable(&frameTable);
  
  resetSimClock(simClock);
  alarm(maxProcessTime);
  pid_t childpid;
  //seed random number generator
  srand(time(0) + getpid());

  //time to create next process  
  sim_clock_t createNext;
  createNext.seconds = 0;
  createNext.nanoseconds = 0;


  //time to print allocation table
  sim_clock_t printNext;
  printNext.seconds = 1;
  printNext.nanoseconds = 0;

  //time between printing allocation table
  sim_clock_t printDelay;
  printDelay.seconds = 1;
  printDelay.nanoseconds = 0;

  int aliveChildren = 0;

  while(1){
    //increment clock
    incrementSimClock(simClock);

    if(compareSimClocks(simClock, &printNext) != -1){
      printAllocationTable();
      sumSimClocks(&printNext, &printDelay);
    }

    //check for messages
    while(1){
      if(deviceFinished(&deviceQueue, simClock)){  //device completed, load page
        int dirty = 0;
        page_t newPage = getPage(&pcb[deviceQueue.head->processId], deviceQueue.head->pageNum);
        page_t oldPage = loadPage(&frameTable, newPage, &dirty);
        if(dirty){
          //write back to pcb pageTable
          writePage(pcb, oldPage);
        }
        //send message
        fprintf(stderr, "OSS: Loaded page[%d] for C[%d]\n", deviceQueue.head->pageNum, deviceQueue.head->processId);
        if(sendMessage(messageQueueId, RESPONSE+deviceQueue.head->processId, deviceQueue.head->processId, 0, 0) == -1)
          perror("OSS: Failed to send message");

        removeHeadDeviceQueue(&deviceQueue, simClock);
        break;
      }
      if(msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), -3000, IPC_NOWAIT) == -1){
        break;
      }
      if(messageBuffer.type > 1000 && messageBuffer.type < 2000){ //REQUEST MESSAGE
        //calculate corresponding page
        int pageNum = getReferenceLocationPageNumber(pcb, messageBuffer.pcbIndex, messageBuffer.address);
        logRequest(messageBuffer.pcbIndex, pageNum);
        //if page is in frame table
        if(pageIsResident(&frameTable, messageBuffer.pcbIndex, pageNum)){
          fprintf(stderr, "OSS: C[%d]'s page is resident\n", messageBuffer.pcbIndex); 
          //set use bit
          setBit(&frameTable.secondChanceBits, messageBuffer.pcbIndex);
          //if writing, set dirty bit
          if(messageBuffer.readOrWrite) setBit(&frameTable.dirtyBits, messageBuffer.pcbIndex);
          //send response
          if(sendMessage(messageQueueId, RESPONSE+messageBuffer.pcbIndex, messageBuffer.pcbIndex, 0, 0) == -1)
            perror("OSS: Failed to send message");
        }
        else{  //page is not in frameTable
          //put in queue and set timer
          insertDeviceQueue(&deviceQueue, messageBuffer.pcbIndex, pageNum, simClock);
          break;
        }
      }
      else if(messageBuffer.type > 2000 && messageBuffer.type < 3000){ //TERMINATE MESSAGE
        logTerminate(messageBuffer.pcbIndex, messageBuffer.address);
        purgeFrameTable(&frameTable, messageBuffer.pcbIndex);
        //issue wait for terminating process
        pcb[messageBuffer.pcbIndex].state = EXIT;
        kill(pcb[messageBuffer.pcbIndex].pid, 2);
        waitpid(-1, NULL, 0);
        //clear 'set' bit
        clearBit(&pcbBitVector, messageBuffer.pcbIndex);
        int index = messageBuffer.pcbIndex;
        
        if(childCounter < MAX_CHILDREN && !isFull(&pcbBitVector)){  //if room for another child
          if(childCounter < PCB_TABLE_SIZE){  //if still creating initial children
            if(compareSimClocks(simClock, &createNext) != -1){  //if time has passed to create next child 
            //create child
              if((childpid = fork()) > 0){  //parent code
                setBit(&pcbBitVector, childCounter);
                initPCB(&pcb[childCounter], childpid, childCounter, simClock);
                childCounter++;
                aliveChildren++;
                fprintf(stderr, "OSS: Created child[%d]\n", childCounter - 1);
                createNext.nanoseconds = (rand() % 501) * 1000000;
              }
              else if(childpid == 0){  //child code
                execl("./child", "./child", "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
              }
              else perror("OSS: Failed to fork");
            }
          }
          else{
            if((childpid = fork()) > 0){  //parent code
              int z;
              for(z = 0; z < PCB_TABLE_SIZE; z++){
                if(pcb[z].state == EXIT) break;
              }
              setBit(&pcbBitVector, z);
              initPCB(&pcb[z], childpid, z, simClock);
              childCounter++;
              fprintf(stderr, "OSS: Replaced child[%d] with %d\n", z, childpid);
            }
            else if(childpid == 0){
              execl("./child", "./child", "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
            }
            else perror("OSS: Failed to fork");
          }
        }
        break;
      }
    }

    if(deviceFinished(&deviceQueue, simClock)){  //device completed, load page
      int dirty = 0;
      page_t newPage = getPage(&pcb[deviceQueue.head->processId], deviceQueue.head->pageNum);
      page_t oldPage = loadPage(&frameTable, newPage, &dirty);
      if(dirty){
        //write back to pcb pageTable
        writePage(pcb, oldPage);
      }
      //send message
      fprintf(stderr, "OSS: Loaded page[%d] for C[%d]\n", deviceQueue.head->pageNum, deviceQueue.head->processId);
      if(sendMessage(messageQueueId, RESPONSE+deviceQueue.head->processId, deviceQueue.head->processId, 0, 0) == -1)
        perror("OSS: Failed to send message");

      removeHeadDeviceQueue(&deviceQueue, simClock);
    }
   
    //Create child
    if(childCounter < MAX_CHILDREN && !isFull(&pcbBitVector)){  //if room for another child
      if(childCounter < PCB_TABLE_SIZE){  //if still creating initial children
        if(compareSimClocks(simClock, &createNext) != -1){  //if time has passed to create next child 
          //create child
          if((childpid = fork()) > 0){  //parent code
            setBit(&pcbBitVector, childCounter);
            initPCB(&pcb[childCounter], childpid, childCounter, simClock);
            childCounter++;
            aliveChildren++;
            fprintf(stderr, "OSS: Created child[%d]\n", childCounter - 1);
            createNext.nanoseconds = (rand() % 501) * 1000000;
          }
          else if(childpid == 0){  //child code
            execl("./child", "./child", "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
          }
          else perror("OSS: Failed to fork");
        }
      }
      else{
        if((childpid = fork()) > 0){  //parent code
          int z;
          for(z = 0; z < PCB_TABLE_SIZE; z++){
            if(pcb[z].state == EXIT) break;
          }
          setBit(&pcbBitVector, z);
          initPCB(&pcb[z], childpid, z, simClock);
          childCounter++;
          fprintf(stderr, "OSS: Replaced child[%d] with %d\n", z, childpid);
        }
        else if(childpid == 0){
          execl("./child", "./child", "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
        }
        else perror("OSS: Failed to fork");
      }
    }

    if(childCounter >= MAX_CHILDREN && isEmpty(&pcbBitVector)) break;  //all children have terminated
  }
  
  raise(SIGINT);

  return 0;
}


