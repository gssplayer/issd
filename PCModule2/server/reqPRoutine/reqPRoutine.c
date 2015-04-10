//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "reqPRoutine.h"

#define NUM_PROCESS_ROUTINE 10

ISSD_Process_Routine_t pRoutines[NUM_PROCESS_ROUTINE];

void* issd_compute_routine(void *arg)
{
	ISSD_ReqPacket_t * rPacket = (ISSD_ReqPacket_t *)arg;
	
	fprintf(stdout, "infname = %s, outfname = %s, devname = %s, opType = %d, req_pid = %jd\n",
			rPacket->ifname, rPacket->ofname, rPacket->devname, rPacket->opType, (intmax_t)rPacket->req_pid );
	fflush(stdout);

	return NULL;
}

static void *thread_routine(void *arg)
{
	int tIdx = (int)arg;
	ISSD_Process_Routine_t *pR = &pRoutines[tIdx];
	ISSD_ReqWork_t *pWork = NULL;


	while(1)
	{
		pthread_mutex_lock(&(pR->queue_lock));
		while(!(pR->reqQueue) && !(pR->shutdown))
		{
			pthread_cond_wait(&(pR->queue_ready), &(pR->queue_lock));
		}

		if(pR->shutdown)
		{
			pthread_mutex_unlock(&(pR->queue_lock));
			pthread_exit(NULL);
		}

		pWork = pR->reqQueue;
		pR->reqQueue = pR->reqQueue->next;
		
		pthread_mutex_unlock(&(pR->queue_lock));
		
		pR->routine(&(pWork->rPacket));
		
		free(pWork);
	}
		
	return NULL;
}

int createProcessRoutines()
{
	int i;

	for(i =0; i < NUM_PROCESS_ROUTINE; i++)
	{
		ISSD_Process_Routine_t *pR;
		pR = &pRoutines[i];

		pR->shutdown = 1;
		
		if(pthread_create(&(pR->thr_id), NULL, thread_routine, (void*)i) != 0)
		{
			fprintf(stderr, "%s: pthread_create failed, errno:%d, err:%s\n", 
				__FUNCTION__, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		pR->reqQueue = NULL;
		
		if(pthread_mutex_init(&(pR->queue_lock), NULL) != 0)
		{
			fprintf(stderr, "%s: pthread_mutex_init failed, errno: %d, err: %s\n",
					__FUNCTION__, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		if(pthread_cond_init(&(pR->queue_ready), NULL) != 0)
		{
			fprintf(stderr, "%s: pthread_cond_init failed, errno:%d, err:%s\n",
					__FUNCTION__, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		pR->routine = issd_compute_routine;
		
	}

	return 0;
}

static void destroyProcessRoutine(int idx)
{
	ISSD_Process_Routine_t * pR = &pRoutines[idx];

	if(pR->shutdown)
		return;

	pR->shutdown = 1;

	// inform waiting pthread
	pthread_mutex_lock(&(pR->queue_lock));
	pthread_cond_signal(&(pR->queue_ready));
	pthread_mutex_unlock(&(pR->queue_lock));

	pthread_join(pR->thr_id, NULL);	
}


void destroyProcessRoutines()
{
	int i;

	for(i =0; i < NUM_PROCESS_ROUTINE; i++)
	{
		destroyProcessRoutine(i);
	}
}

int addReqWork2Routine(ISSD_ReqPacket_t* rPacket ,int idx)
{
	ISSD_Process_Routine_t * pR = &pRoutines[idx];

	ISSD_ReqWork_t *work = NULL, *member = NULL;

	work = malloc(sizeof(ISSD_ReqWork_t));
	if(!work)
	{
		printf("%s:malloc failed\n", __FUNCTION__);
		return -1;
	}	
	
	work->rPacket = *rPacket;
	work->next = NULL;

	pthread_mutex_lock(&(pR->queue_lock));
	member = pR->reqQueue;
	if(member == NULL)
	{
		pR->reqQueue = work;
	}
	else
	{
		while(member->next)
		{
			member = member->next;
		}

		member->next = work;
	}
	pthread_cond_signal(&(pR->queue_ready));
	pthread_mutex_unlock(&(pR->queue_lock));
	
	return 0;
}

// current just for test
int thrIdx = 0;
int getProcessRoutineIdx(char* devname)
{
	if(!devname)
		return -1;

	return (thrIdx + 1) % NUM_PROCESS_ROUTINE;  
}
