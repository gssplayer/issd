#ifndef _H_ISSD_H

#define _H_ISSD_H

#define ISSD_SUPPORT_FALLOCATE 1
#define TEST_EXTRACT_LBA 0
#define TEST_OUT_FILE 	0
#define TEST_READ_BUFFER 1

// computing ops list 
#define ISSD_FILECOPY 0
#define ISSD_MERGE 1

#define ISSD_MAX_SEQ_BLOCK 30
#define ISSD_MAX_RD_BLOCKS ISSD_MAX_SEQ_BLOCK
#define ISSD_MAX_WR_BLOCKS ISSD_MAX_SEQ_BLOCK

#define ISSD_MAX_OP_FILES 3

typedef unsigned int sector_t;
typedef unsigned int UINT32;

typedef struct lbaExt{
	UINT32 lba;
	UINT32 len;
} lbaExt_t;

// active flash command type
// note: packet size <= 512
typedef struct afCommand{
	int num_packet;
	int opType;
	int isOK;

	// file extents
	int num_files; // num_files <= ISSD_MAX_OP_FILES
	UINT32 fileSize[ISSD_MAX_OP_FILES];
	UINT32 file_ext_offset[ISSD_MAX_OP_FILES + 1];
	lbaExt_t fileExtArr[0];	
} afCommand_t;

#define MAX_AFCMD_PACKET_SIZE 32 * 1024
#define MAX_AFCMD_MAINPARA_SIZE 512

typedef struct {
	char devname[20];
	int fs_blksize;
	int fs_blkbits;
	int part_start;
} issd_env_t;

#define DEVICE_NAME "/dev/sdb1"

#define SECTOR_SIZE 512

extern int issd_compute(int opType, char *ifname, char* ofname, char* devname, ...);

extern int issd_createCmd(afCommand_t *pCmd, char* ifname, char* ofname, issd_env_t *env);

extern int issd_sendCmd(afCommand_t *pCmd, int cmdSize, char* devname);

extern int issd_waitFinish(afCommand_t *pCmd, char* devname);

extern int sata_writeBuffer(void* pCmd, int size, char* devname);

extern int sata_readBuffer(void *rbuffer, char* devname);
#endif
