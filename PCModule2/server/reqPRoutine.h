#ifndef _H_REQPROUTINE_H
#define _H_REQPROUTINE_H

#include <pthread.h>
#include "issd_server.h"

typedef struct ISSD_ReqWork
{
	ISSD_ReqPacket_t rPacket;
	struct ISSD_ReqWork *next;
} ISSD_ReqWork_t;

typedef struct ISSD_Process_Routine
{
	int shutdown;
	pthread_t thr_id;
	ISSD_ReqWork_t *reqQueue;
	pthread_mutex_t queue_lock;
	pthread_cond_t queue_ready;
	
	void* (*routine)(void *);
	
} ISSD_Process_Routine_t;

#endif
