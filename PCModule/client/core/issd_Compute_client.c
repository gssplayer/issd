#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <assert.h>
#include <limits.h>

#include "issd_client.h"
#include "atomic.h"

#define SHM_NUM_ITEMS 10
#define SHM_NAME "/issd_shm"
#define CLIENT_FIFO_NAME "/tmp/issd_cli_%d_%d_fifo"
int shm_num_items = 10;

static sem_t *full, *empty;
static sem_t *mutex_producer, *mutex_consumer;
static ISSD_ReqPacket_t *shared_buffer;
static int *pWrIdx, *pRdIdx;

// reference count, represent times to invoke issd_comput_init
static atomic_t issd_init_inv = ATOMIC_INIT(0);

// request sequence per process
// so pid + reqSeq can identify each request.
static atomic_t issd_reqSeqPerProcess = ATOMIC_INIT(0); 

static int getNextReqSeq(void)
{
	#define ATOMIC_MAX_LIMIT 0xffffff
	// when atomic var increments to ATOMIC_MAX_LIMIT, reback to zero.
	atomic_cas(&issd_reqSeqPerProcess, ATOMIC_MAX_LIMIT, 0);
	
	// we need return val , so packaged atomic functions don't satisfy
	return __sync_add_and_fetch(&(issd_reqSeqPerProcess.counter), 1);
}

/*
 * Note:
 * 1, issd_compute_init() and  issd_compute_destroy() must be paired.
 * 2, issd_compute_init() and issd_compute_destroy() are not thread_safe, so in multi-thread environment,
 *    you need a global lock to ensure them mutually exclusive.
 * 3, So, I suggest you invoke issd_compute_init when process creates, and issd_compute_destroy when process exits.   
 *
 */

int issd_compute_init()
{
	// if invoked
	if(!atomic_cas(&issd_init_inv, 0, 1))
	{
		atomic_inc(&issd_init_inv);
		return 0;
	}

	int fd;
	char *ptr = NULL;
	struct stat sb;

	fd = shm_open(SHM_NAME, O_RDWR, 0);
	if(fd == -1)
	{
		perror("shm_open");
		return -1;
	}	

	if(fstat(fd, &sb) == -1)
	{
		close(fd);
		perror("fstat");
		return -1;
	}

	if((ptr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
	{
		close(fd);
		perror("mmap");
		return -1;
	}

	
	close(fd);

	full = (sem_t *)ptr;
	empty = ((sem_t*)ptr) + 1;
	mutex_producer = ((sem_t*)ptr) + 2;
	mutex_consumer = ((sem_t*)ptr) + 3;
	pWrIdx = (int *)(((sem_t*)ptr) + 4);
	pRdIdx = pWrIdx + 1;
	shm_num_items = *(pRdIdx + 1);
	shared_buffer = (ISSD_ReqPacket_t*)(pRdIdx + 2);
	
	printf("shm_num_items = %d\n", shm_num_items);	
	printf("issd_compute_init successful!\n");
	fflush(stdout);		
	return 0;
}

static void sendReqPacket(ISSD_ReqPacket_t* rPacket)
{
	sem_wait(empty);
	sem_wait(mutex_producer);
	
	shared_buffer[*pWrIdx] = *rPacket;
	
	*pWrIdx = (*pWrIdx + 1) % shm_num_items;
	
	sem_post(mutex_producer);
	sem_post(full);

}


//static ISSD_ReqPacket_t rPacket;
/*
 * Note: ofname must be absolute pathname
 */
int issd_compute(char* ifname, char* ofname, int opType)
{
	if(!ifname || !ofname)
	{
		fprintf(stderr, "in or out filename is NULL!\n");
		return -1;
	}
	
//	printf("path_max:%d\n", PATH_MAX);		

	ISSD_ReqPacket_t rPacket;
	
//	printf("ifname:%s, ofname:%s\n", ifname, ofname);
	
	char* rPathname = NULL;	
	// convert ifname to absolute pathname
	if((rPathname =realpath(ifname, NULL))!=NULL)
	{
		snprintf(rPacket.ifname, MAX_FILENAME_LEN, "%s", rPathname);
		printf("absolute pathname:%s:%s\n", ifname, rPacket.ifname);
		free(rPathname);
	}
	
	
//	printf("In here!\n");	
	snprintf(rPacket.ofname, MAX_FILENAME_LEN, "%s", ofname);
	
	
	// get devname from ifname
	struct stat sb;
	if(stat(ifname, &sb))
	{
		perror("stat");
		return -1;
	}

	char buf[MAX_DEVNAME_LEN];
	if(bdevname(sb.st_dev, buf))
	{
		snprintf(rPacket.devname, MAX_DEVNAME_LEN, "/dev/%s", buf);
		printf("devname:%s\n", rPacket.devname);
	}

	rPacket.opType = opType;
	rPacket.req_pid = getpid();
	rPacket.reqSeq = getNextReqSeq();
	
	fprintf(stdout, "infile:%s, outfile:%s, opType:%d, pid:%d, reqSeq:%d\n", rPacket.ifname, rPacket.ofname,
			rPacket.opType, rPacket.req_pid, rPacket.reqSeq);
	
	char client_fifo[256];
	sprintf(client_fifo, CLIENT_FIFO_NAME, rPacket.req_pid, rPacket.reqSeq);
	
	if(mkfifo(client_fifo, 0777) == -1)
	{
		perror("mkfifo");
		return -1;
	}
	
	sendReqPacket(&rPacket);

	ISSD_FeedbackPacket_t fdPack;
	int client_fifo_fd = open(client_fifo, O_RDONLY);
	if(client_fifo_fd == -1)
	{
		perror("open");
		unlink(client_fifo);
		return -1;
	}
	
	if(read(client_fifo_fd, &fdPack, sizeof(ISSD_FeedbackPacket_t)) > 0)
	{
		if(fdPack.isOk)
		{
			printf("issd_%d_%d req successful!\n", rPacket.req_pid, rPacket.reqSeq);
		}
		else
		{
			printf("issd_%d_%d req failed\n", rPacket.req_pid, rPacket.reqSeq);
		}
		
		close(client_fifo_fd);
		unlink(client_fifo);
		return fdPack.isOk;
	}

	fprintf(stderr, "read fifo failed\n");
	close(client_fifo_fd);
	unlink(client_fifo);
	return -1;

}

int issd_compute_destroy()
{
	// destroy when issd_compute_destroy is invoked at the last time
	if(!atomic_cas(&issd_init_inv, 1, 0))
	{
		atomic_dec(&issd_init_inv);
		return 0;
	}

	full = NULL;
	empty = NULL;
	mutex_producer = NULL;
	mutex_consumer = NULL;
	pWrIdx = NULL;
	pRdIdx = NULL;
	shared_buffer = NULL;

	return 0;	
}
