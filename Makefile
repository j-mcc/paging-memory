CC = gcc
DEPS = simulatedclock.h pcb.h bitarray.h paging.h oss.h messagequeue.h
CFLAGS = -g -I.

OSS = oss
OSSOBJS = oss.o simulatedclock.o pcb.o bitarray.o paging.a messagequeue.o
OSSLIBS = -pthread -lm

PAGING = paging.a
PAGINGOBJS = paging.o bitarray.o
PAGINGLIBS = -lm

CHILD = child
CHILDOBJS = child.o simulatedclock.o pcb.o paging.a messagequeue.o
CHILDLIBS = -pthread -lm


all: $(PAGING) $(OSS) $(CHILD)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $<


$(OSS): $(OSSOBJS) 
	$(CC) -o $(OSS) $(OSSOBJS) $(OSSLIBS) $(CFLAGS)

$(CHILD): $(CHILDOBJS)
	$(CC) -o $(CHILD) $(CHILDOBJS) $(CHILDLIBS) $(CFLAGS)

$(PAGING): $(PAGINGOBJS)
	ar cr $(PAGING) $(PAGINGOBJS)

clean: 
	/bin/rm -f $(OSS) $(CHILD) *.a *.o *.txt
