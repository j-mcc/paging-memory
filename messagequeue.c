 /*
 * $Author: o1-mccune $
 * $Date: 2017/12/01 20:08:00 $
 * $Revision: 1.2 $
 * $Log: messagequeue.c,v $
 * Revision 1.2  2017/12/01 20:08:00  o1-mccune
 * updated sendMessage
 *
 * Revision 1.1  2017/11/26 21:52:51  o1-mccune
 * Initial revision
 *
 */


#include "oss.h"
#include "messagequeue.h"
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int messageQueueId;

int getMessageQueue(int key){
  if((messageQueueId = msgget(key, IPC_CREAT | 0644)) == -1){
    return -1;
  }
  return messageQueueId; 
}

int sendMessage(const int messageQueueId, const long messageType, const int pcbIndex, const int address, const int readOrWrite){
  int error = 0;
  message_t *message;
  if((message = malloc(sizeof(message_t))) == NULL) return -1;

  message->type = messageType;
  message->pcbIndex = pcbIndex;
  message->address = address; 
  message->readOrWrite = readOrWrite; 

  if(msgsnd(messageQueueId, message, sizeof(message_t), 0) == -1) error = errno;
  free(message);
  if(error){
    errno = error;
    return -1;
  }
  return 0;
}

int removeMessageQueue(int messageQueueId){
  return msgctl(messageQueueId, IPC_RMID, NULL);
}
