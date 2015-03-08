
CC = gcc
LD = gcc

CFLAGS = -g -O2 -Wall

OBJS = issd_server_main.o initlog.o reqQueue.o reqPRoutine.o issd_compute.o sataCom.o

all: $(OBJS)
	$(LD) $(CFLAGS) -o issd_compute_server $^ -lrt -pthread -lm

issd_server_main.o: issd_server_main.c
	$(CC) $(CFLAGS) -o $@ -c $<

initlog.o: initlog.c
	$(CC) $(CFLAGS) -o $@ -c $<

reqQueue.o: reqQueue.c
	$(CC) $(CFLAGS) -o $@ -c $< -lrt
reqPRoutine.o: reqPRoutine.c
	$(CC) $(CFLAGS) -o $@ -c $< -pthread
issd_compute.o: issd_compute.c
	$(CC) $(CFLAGS) -o $@ -c $<
sataCom.o: sataCom.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	rm -f issd_compute_server *.o
