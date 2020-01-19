CC = gcc
CFLAGS = -Wall -g -I.
MM = mm.c

OBJS = usemem.o mm.o

COMPACT = 

all: usemem

usemem: $(OBJS)
	$(CC) $(CFLAGS) -o usemem $(OBJS) -lm

mm.o: $(MM) mm.h
	$(CC) $(CFLAGS) -c -o mm.o $(MM)

clean:
	rm -f *~ *.o usemem

test60: usemem
	./usemem --single $(COMPACT)
	./usemem --single --s 4000 $(COMPACT)
	./usemem --singles $(COMPACT)
	./usemem --singles --s 1 $(COMPACT)
	./usemem --singles --s 37 $(COMPACT)
	./usemem --singles --singles --s 32 --n 128 $(COMPACT)
	./usemem --singles --singles --s 32 --n 126 $(COMPACT)
	./usemem --excessive $(COMPACT)
	./usemem --shrinking $(COMPACT)
	./usemem --shrinking --s 32 $(COMPACT)
	./usemem --shrinking --s 256 $(COMPACT)

test80: usemem
	$(MAKE) test60
	./usemem --growing $(COMPACT)
	./usemem --growing --s 1 $(COMPACT)
	./usemem --growing --s 256 $(COMPACT)

test90: usemem
	$(MAKE) test80
	$(MAKE) test80 COMPACT=--compact

test100: usemem
	$(MAKE) test90
	./usemem --timing
	./usemem --timing --compact
	./usemem --timing --n 10000
	./usemem --timing --n 10000 --compact
