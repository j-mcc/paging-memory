 /*
 * $Author: o1-mccune $
 * $Date: 2017/12/02 05:41:21 $
 * $Revision: 1.2 $
 * $Log: child.c,v $
 * Revision 1.2  2017/12/02 05:41:21  o1-mccune
 * added calculation for total sys time.
 *
 * Revision 1.1  2017/12/01 20:07:18  o1-mccune
 * Initial revision
 *
 */

#include "oss.h"
#include "messagequeue.h"
#include "paging.h"
#include "pcb.h"
#include "simulatedclock.h"
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>


static int memoryLocation;
static int readOrWrite;
static int numMemoryReferences = 0;
static int nextTerminate;

static key_t messageQueueKey;
static int messageQueueId;
static int messageId;
static int pcbId;
static int sharedClockId;
static sim_clock_t *simClock;
static pcb_t *pcb;
static message_t messageBuffer;

static void printOptions(){
  fprintf(stderr, "CHILD:  Command Help\n");
  fprintf(stderr, "\tCHILD:  Optional '-h': Prints Command Usage\n");
  fprintf(stderr, "\tCHILD:  '-p': Shared memory ID for PCB Table.\n");
  fprintf(stderr, "\tCHILD:  '-m': Shared memory ID for message.\n");
  fprintf(stderr, "\tCHILD:  '-c': Shared memory ID for clock.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  int nCP = 0;
  while ((c = getopt (argc, argv, "hp:c:m:")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 'c':
        sharedClockId = atoi(optarg);
        break;
      case 'm':
        messageQueueId = atoi(optarg);
        break;
      case 'p':
	pcbId = atoi(optarg);
        break;
      case '?':
	if(isprint (optopt))
          fprintf(stderr, "CHILD: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "CHILD: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  return 0;
}

static int detachPCB(){
  return shmdt(pcb);
}

static int detachSharedClock(){
  return shmdt(simClock);
}

static int attachPCB(){
  if((pcb = shmat(pcbId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedClock(){
  if((simClock = shmat(sharedClockId, NULL, SHM_RDONLY)) == (void *)-1) return -1;
  return 0;
}

static void cleanUp(int signal){
  if(pcb)
    if(detachPCB() == -1) perror("CHILD: Failed to detach PCB table");
  if(simClock)
    if(detachSharedClock() == -1) perror("CHILD: Failed to detach shared clock");
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(0);
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

static int generateExitCondition(){
  int range = rand() % 101;
  int sign = pow(-1, rand() % 2);
  return (1000 + (sign * range));
}

int main(int argc, char **argv){
  parseOptions(argc, argv);

  if(initAlarmWatcher() == -1){
    perror("CHILD: Failed to init SIGALRM watcher");
    exit(1);
  }
  if(initInterruptWatcher() == -1){
    perror("CHILD: Failed to init SIGINT watcher");
    exit(2);
  }
  if(attachPCB() == -1){
    perror("CHILD: Failed to attach PCB table");
    exit(4);
  }
  if(attachSharedClock() == -1){
    perror("CHILD: Failed to attach clock");
    exit(5);
  }

  srand(time(0) + getpid());  //seed random number
  int i = findPCB(pcb, getpid()); //find pcb index  

  sim_clock_t processEndTime;
  processEndTime.seconds = 2;
  processEndTime.nanoseconds = 0;
  sumSimClocks(&processEndTime, simClock);


  int exitCheckCondition = generateExitCondition();
  //fprintf(stderr, "CHILD[%d] exit condition %d\n", i, exitCheckCondition);
   
  sim_clock_t eventTime;
  
  //fprintf(stderr, "CHILD[%d] Has %d pages\n", i, pcb[i].pageTable.numPages);  

  while(1){
     
    int referenceLocation = (rand() % (PAGE_SIZE + 1)) * pcb[i].pageTable.numPages;
    int readOrWrite = rand() % 2;
    
//    if(!readOrWrite) fprintf(stderr, "CHILD[%d] Requesting location %d for READ\n", i, referenceLocation);  
//    else fprintf(stderr, "CHILD[%d] Requesting location %d for WRITE\n", i, referenceLocation);  
    
    if(sendMessage(messageQueueId, REQUEST+i, i, referenceLocation, readOrWrite) == -1) perror("\t\t\t\tCHILD: Failed to send message");

    //wait for response
    msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), RESPONSE+i, 0);     
    numMemoryReferences++;

    if(numMemoryReferences >= exitCheckCondition){  //start checking for termination
      exitCheckCondition += generateExitCondition();
      int terminate = rand() % 2;
      if(terminate == 0){
        pcb[i].timeInSystem = findDifference(simClock, &pcb[i].timeCreated);
        if(sendMessage(messageQueueId, TERMINATE+i, i, numMemoryReferences, 0) == -1) perror("CHILD: Failed to send message");
        break;
      }
    }
  }
  cleanUp(2);

  return 0;
}

