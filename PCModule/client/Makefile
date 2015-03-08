CC = gcc
LD = gcc

CFLAGS = -g -O2 -Wall

INCLUDEDIR = core/
LIBDIR = core

OBJS = test_client_main.o issd_Compute_client.o getDevname.o

all: $(OBJS)
	$(LD) $(CFLAGS)   -o issd_test_client $^ -lrt
test_client_main.o: test_client_main.c
	$(CC) $(CFLAGS) -o $@ -c -I $(INCLUDEDIR)  $<
issd_Compute_client.o: $(LIBDIR)/issd_Compute_client.c
	$(CC)  $(CFLAGS) -o $@ -c -I $(INCLUDEDIR) $<  -lrt
getDevname.o: $(LIBDIR)/getDevname.c
	$(CC) $(CFLAGS) -o $@ -c -I $(INCLUDEDIR) $<

.PHONY: clean

clean:
	rm -f issd_test_client *.o
