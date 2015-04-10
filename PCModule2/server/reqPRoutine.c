//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include "reqPRoutine.h"
#include "issd.h"

#define NUM_PROCESS_ROUTINE 10
int num_process_routine = 10;

#define CLIENT_FIFO_NAME "/tmp/issd_cli_%d_%d_fifo"

//ISSD_Process_Routine_t pRoutines[NUM_PROCESS_ROUTINE];
ISSD_Process_Routine_t *pRoutines = NULL;

typedef struct devEntry {
	char devname[20];
	int used; // 1: using, 0:unused
	int flag; // if accessed before current timer expires
} devEntry_t;

static int timer_expired = 0;

devEntry_t *dev2thd = NULL;

int issd_feedback(ISSD_ReqPacket_t *rPacket, int flag)
{
	ISSD_FeedbackPacket_t fdPack;
	fdPack.isOk = flag;

	char client_fifo[256];
	sprintf(client_fifo, CLIENT_FIFO_NAME, rPacket->req_pid, rPacket->reqSeq);

	int client_fifo_fd = open(client_fifo, O_WRONLY);
	if(client_fifo_fd == -1)
	{
		fprintf(stderr, "can't feedback request from %d, seq:%d\n", rPacket->req_pid, rPacket->reqSeq);
		fflush(stderr);
		return -1;
	}

	write(client_fifo_fd, &fdPack, sizeof(ISSD_FeedbackPacket_t));
	close(client_fifo_fd);
	
	return 0;			
}

void* issd_compute_routine(void *arg)
{
	ISSD_ReqPacket_t * rPacket = (ISSD_ReqPacket_t *)arg;
	
	fprintf(stdout, "devname = %s, opType = %d, req_pid = %jd\n, reqSeq:%d\n",
			 rPacket->devname, rPacket->opType, (intmax_t)rPacket->req_pid, rPacket->reqSeq );
	fflush(stdout);

	int ret;
	switch(rPacket->opType)
	{
		case ISSD_FILECOPY:
		{
			ret = issd_compute(rPacket->opType, rPacket->ifname[0], rPacket->ofname, rPacket->devname);
			break;	
		}
		case ISSD_MERGE:
		{
			ret = issd_compute(rPacket->opType, rPacket->ifname[0], rPacket->ofname, rPacket->devname,
					rPacket->ifname[1]);
			break;
		}
		default:
			break;
	}
	issd_feedback(rPacket, !ret);

	return NULL;
}

static void *thread_routine(void *arg)
{
	int tIdx = (int)arg;
	ISSD_Process_Routine_t *pR = &pRoutines[tIdx];
	ISSD_ReqWork_t *pWork = NULL;


	while(1)
	{
/*		printf("thread:%d, running\n");
		fflush(stdout);*/
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
		
		//ISSD_ReqPacket_t * rPacket = &(pWork->rPacket);
/*		char *strtail = strchr(rPacket->devname, '\0');
		*strtail = (tIdx + '0');
		strtail++;
		*strtail = '\0';*/
				
		pR->routine(&(pWork->rPacket));
		
		free(pWork);
	}
		
	return NULL;
}

#define DEV_MAP_REFRESH_TIME (60 * 60)  // 1 hour

void alarm_handler(int signum)
{
	timer_expired = 1;
	alarm(DEV_MAP_REFRESH_TIME);	
}
static void init_dev2Routine()
{
	dev2thd = (devEntry_t*)malloc(num_process_routine * 2 * sizeof(devEntry_t));
	if(dev2thd == NULL)
	{
		fprintf(stderr, "malloc failed\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	
	memset(dev2thd, 0, sizeof(devEntry_t) *(2* num_process_routine));
	signal(SIGALRM, alarm_handler);
	alarm(DEV_MAP_REFRESH_TIME);

}

int createProcessRoutines()
{
	int i;
	printf("before create process routines\n");
	fflush(stdout);

	printf("num_process_routine = %d\n", num_process_routine);
	fflush(stdout);

	pRoutines = (ISSD_Process_Routine_t *) malloc(num_process_routine * sizeof(ISSD_Process_Routine_t));
	if(pRoutines == NULL)
	{
		fprintf(stderr, "malloc failed\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}		

	for(i =0; i < num_process_routine; i++)
	{
		ISSD_Process_Routine_t *pR;
		pR = &pRoutines[i];

		pR->shutdown = 0;
		
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
		
		if(pthread_create(&(pR->thr_id), NULL, thread_routine, (void*)i) != 0)
		{
			fprintf(stderr, "%s: pthread_create failed, errno:%d, err:%s\n", 
				__FUNCTION__, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

	}

	init_dev2Routine();
	
	printf("create process Routines done!\n");
	fflush(stdout);
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

	for(i =0; i < num_process_routine; i++)
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
		fprintf(stderr, "%s:malloc failed\n", __FUNCTION__);
		fflush(stderr);
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


// map from dev to routine

// current just for test
//int thrIdx = 0;
int getProcessRoutineIdx(char* devname)
{
	if(!devname)
		return -1;
	// for example, /dev/sdb1, to subtract sdb
	char realDev[20];
	strcpy(realDev, devname+5);
	char *strtail = strchr(realDev, '\0');
	if(isdigit(*(strtail-1)))
		strtail--;
	*strtail = '\0';
	
	fprintf(stdout, "devname:%s, to realDev:%s\n", devname, realDev);
	fflush(stdout);
	int i;
	// if timer_expired = 1, to refresh dev map list
	if(timer_expired) 
	{
		for(i = 0; i < 2*num_process_routine; i++)
		{
			// if finding a devEntry not be accssed before timer expires, flag it unused
			if(dev2thd[i].used && !dev2thd[i].flag)
			{
				dev2thd[i].used = 0;
			}
			dev2thd[i].flag = 0;
		}
		timer_expired = 0;	
	}

	
	int first_unused = -1;
	for(i = 0; i < 2 * num_process_routine; i++)
	{
		if(!dev2thd[i].used && first_unused == -1)
		{
			first_unused = i;
			continue;
		}
		if(dev2thd[i].used && !strcmp(dev2thd[i].devname, realDev))
		{
			// found
			dev2thd[i].flag = 1;
			return (i % num_process_routine);
		}
	}
// can't find unused space for storing (dev,thread) pair, next we should check dev2thd map list and free unvalid pairs 
	if(first_unused == -1) return -1;
	
	// insert
	strcpy(dev2thd[first_unused].devname, realDev);
	dev2thd[first_unused].used = 1;
	dev2thd[first_unused].flag = 1;

	return (first_unused % num_process_routine);

	// not found
	
//	thrIdx =  (thrIdx + 1) % NUM_PROCESS_ROUTINE;  
//	return thrIdx;
}
