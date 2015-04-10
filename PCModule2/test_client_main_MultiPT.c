// client test entry

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include "issd_lib.h"

#define NUM_PROCESS 5
#define PRODUCER_NUM 10
#define TIMES_PER_PRODUCER 10

void* producer(void* arg)
{
	int thIdx = (int)arg;
	int i;
//	issd_compute_init();
	
	char ifname[1024];
	char ofname[1024];

	for(i = 0; i < TIMES_PER_PRODUCER; i++)
	{
		snprintf(ifname, 1024,"ifile-%jd-%d", (intmax_t)getpid(), thIdx);
		snprintf(ofname, 1024, "ofile-%jd-%d", (intmax_t)getpid(), thIdx);
		int opType = i;
		
		issd_compute(ifname, ofname, opType);
	}
	
//	issd_compute_destroy();
	pthread_exit(NULL);
}

void test_routine(void)
{
	pthread_t thds[PRODUCER_NUM];
	
	issd_compute_init();

	char logname[256];
	sprintf(logname, "log/client_log%d", getpid());
	close(1);
	close(2);
	open(logname, O_CREAT | O_WRONLY | O_APPEND, 0x777);
	dup(1);

	int i;
	for(i =0; i < PRODUCER_NUM; i++)
	{
		pthread_create(&thds[i], NULL, producer, (void *)i);
	}

	for(i = 0; i < PRODUCER_NUM; i++)
	{
		pthread_join(thds[i], NULL);
	}
	
	issd_compute_destroy();
}

int process_create(pid_t *pid_new, void (*routine)(void))
{
	pid_t pid;

	switch(pid = fork())
	{
		// err
		case -1:
		{
			return errno;
			break;
		}
		// child process
		case 0:
		{
			routine();
			exit(0);
			break;
		}
		// parent process
		default:
		{
			*pid_new = pid;
			break;
		}
	}

	return 0;
}

int main(int argc, char** argv)
{
	int i;
	pid_t testProcesses[PRODUCER_NUM];

	for(i = 0; i < NUM_PROCESS; i++)
	{
		process_create(&testProcesses[i], test_routine);
	}

	for(i = 0; i < NUM_PROCESS; i++)
	{
		waitpid(0, NULL, 0);
	}

	return 0;
}


