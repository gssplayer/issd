#ifndef _H_ISSD_CLIENT_H

#define MAX_FILENAME_LEN 1024
#define MAX_DEVNAME_LEN 20

#include <unistd.h>
#include <sys/types.h>

typedef struct ISSD_ReqPacket
{
	char ifname[MAX_FILENAME_LEN];
	char ofname[MAX_FILENAME_LEN];
	char devname[MAX_DEVNAME_LEN];
	int opType;
	pid_t req_pid;
	int reqSeq;
} ISSD_ReqPacket_t;

typedef struct ISSD_FeedbackPacket
{
	int isOk;
} ISSD_FeedbackPacket_t;

extern char *bdevname(dev_t dev, char* devname);

#endif
