#ifndef _H_ISSD_SERVER_H
#define _H_ISSD_SERVER_H

#define LOG_FILENAME "/var/log/issd/issd.log"
#define LOG_ERR_FILENAME "/var/log/issd/issdErr.log"

#define LOG_REQ_PACKET 0

#include <unistd.h>
#include <sys/types.h>
#define MAX_FILENAME_LEN 1024
#define MAX_DEVNAME_LEN 20

typedef struct ISSD_ReqPacket
{
	char ifname[MAX_FILENAME_LEN];
	char ofname[MAX_FILENAME_LEN];
	char devname[MAX_DEVNAME_LEN];
	int opType;
	pid_t req_pid; // request process id;
	int reqSeq; // request sequence
} ISSD_ReqPacket_t;

typedef struct ISSD_FeedbackPacket
{
	int isOk;
} ISSD_FeedbackPacket_t;

extern int shm_num_items;

extern int num_process_routine;

extern void init_log();

extern void createRQueue();

extern void getNextReqPacket(ISSD_ReqPacket_t *rPacket);

extern void dispatchReqPacket(ISSD_ReqPacket_t *rPacket);

extern int createProcessRoutines();

extern void destroyProcessRoutines();

extern int addReqWork2Routine(ISSD_ReqPacket_t* rPacket, int idx);

extern int getProcessRoutineIdx(char* devname);

extern int issd_feedback(ISSD_ReqPacket_t * rPacket, int flag);
#endif
