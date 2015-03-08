// functions create/receive/dispatch reqPacket
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include "issd_server.h"

#define SHM_NUM_ITEMS 10
#define SHM_NAME "/issd_shm"

int shm_num_items = 10;
static sem_t *full, *empty;
static sem_t *mutex_producer, *mutex_consumer;
static ISSD_ReqPacket_t *shared_buffer;
static int *pWrIdx, *pRdIdx; // current read/write item pointer  



void createRQueue()
{
	int length;
	int fd;
	void *ptr = NULL;

	// caculate shm size needed
	length = 4*sizeof(sem_t) + 3*sizeof(int) + shm_num_items * sizeof(ISSD_ReqPacket_t);
	
	printf("shm length = %d\n", length);	
	fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0666);
	if(fd == -1)
	{
		perror("shm_open");
		goto failed;
	}

	if(ftruncate(fd, length) == -1)
	{
		perror("fruncate");
		goto failed_rm_shm;
	}

	if((ptr = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
	{
		perror("mmap");
		goto failed_rm_shm;
	}
	
	close(fd);

	full = (sem_t*)ptr;
	empty = ((sem_t*)ptr) + 1;
	mutex_producer = ((sem_t*)ptr) + 2;
	mutex_consumer = ((sem_t*)ptr) + 3;
	pWrIdx = (int*)(((sem_t*)ptr) + 4);
	pRdIdx = pWrIdx + 1;
	shared_buffer = (ISSD_ReqPacket_t *)(pRdIdx + 2);

	// init
	sem_init(full, 1, 0);
	sem_init(empty, 1, shm_num_items);
	sem_init(mutex_producer, 1, 1);
	sem_init(mutex_consumer, 1, 1);
	*pWrIdx = 0;
	*pRdIdx = 0;
	*(pRdIdx + 1) = shm_num_items;
	
	return;
failed_rm_shm:
	close(fd);
	shm_unlink(SHM_NAME);

failed:
	exit(-1);
	
}

void getNextReqPacket(ISSD_ReqPacket_t *rPacket)
{
	sem_wait(full);
	sem_wait(mutex_consumer);

	*rPacket = shared_buffer[*pRdIdx];

	*pRdIdx = (*pRdIdx + 1) % shm_num_items;
	
	sem_post(mutex_consumer);
	sem_post(empty);
	
#if LOG_REQ_PACKET
	fprintf(stdout, "infname = %s, outfname = %s, devname = %s, opType = %d, req_pid = %jd\n",
			rPacket->ifname, rPacket->ofname, rPacket->devname, rPacket->opType, (intmax_t)rPacket->req_pid );
	fflush(stdout);
#endif
}

void dispatchReqPacket(ISSD_ReqPacket_t *rPacket)
{
	int idx = getProcessRoutineIdx(rPacket->devname);
	printf("%s:%d\n", rPacket->devname, idx);
		
/*	char* strtail = strchr(rPacket->devname, '\0');
	*strtail = (idx + '0');
	strtail++;
	*strtail = '\0';*/	
	if(addReqWork2Routine(rPacket, idx) != 0)
	{
		issd_feedback(rPacket, 0);
	}	
}
