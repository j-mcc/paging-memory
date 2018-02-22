
/* 
 * $Author: o1-mccune $
 * $Date: 2017/12/01 20:08:31 $
 * $Revision: 1.2 $
 * $Log: messagequeue.h,v $
 * Revision 1.2  2017/12/01 20:08:31  o1-mccune
 * updated message_t
 *
 * Revision 1.1  2017/11/26 21:51:15  o1-mccune
 * Initial revision
 *
 */

#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#define RESPONSE 3001
#define TERMINATE 2001
#define REQUEST 1001

typedef struct{
  long type;
  unsigned int pcbIndex;
  unsigned int address;
  unsigned int readOrWrite;
}message_t;

int getMessageQueue(int key);

int removeMessageQueue(int messageQueueId);

int sendMessage(const int messageQueueId, const long messageType, const int pcbIndex, const int address, const int readOrWrite);

#endif
