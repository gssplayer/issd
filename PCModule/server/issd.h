#ifndef _H_ISSD_H

#define _H_ISSD_H

#define ISSD_SUPPORT_FALLOCATE 1
#define TEST_EXTRACT_LBA 0
#define TEST_OUT_FILE 	0
#define TEST_READ_BUFFER 1

// computing ops list 
#define ISSD_BACKUP 0

#define ISSD_MAX_SEQ_BLOCK 30
#define ISSD_MAX_RD_BLOCKS ISSD_MAX_SEQ_BLOCK
#define ISSD_MAX_WR_BLOCKS ISSD_MAX_SEQ_BLOCK

typedef unsigned int sector_t;
typedef unsigned int UINT32;


// active flash command type
// note: packet size <= 512
typedef struct afCommand{
	int opType;
	UINT32 fileSize; // in: input file size, out: output file size
	
	UINT32 nr_rd_bios; // number of read sequence blocks, note:nr_rd_bios < ISSD_MAX_SEQ_BLOCK
	UINT32 rBios[ISSD_MAX_RD_BLOCKS];
	UINT32 rBioLen[ISSD_MAX_RD_BLOCKS];

	UINT32 nr_wr_bios; // number of write sequence blocks, note: nr_wr_bios < ISSD_MAX_SEQ_BLOCK
	UINT32 wBios[ISSD_MAX_WR_BLOCKS];
	UINT32 wBioLen[ISSD_MAX_WR_BLOCKS];

	int isOK; // read back by host for judging if issd_process_cmd successful.

} afCommand_t;

typedef struct {
	char devname[20];
	int fs_blksize;
	int fs_blkbits;
	int part_start;
} issd_env_t;

#define DEVICE_NAME "/dev/sdb1"

#define SECTOR_SIZE 512


extern int issd_compute(int opType, char *ifname, char* ofname, char* devname);

extern int issd_createCmd(afCommand_t *pCmd, char* ifname, char* ofname, issd_env_t *env);

extern int issd_sendCmd(afCommand_t *pCmd, char* devname);

extern int issd_waitFinish(afCommand_t *pCmd, char* devname);

extern int sata_writeBuffer(void* pCmd, int size, char* devname);

extern int sata_readBuffer(void *rbuffer, char* devname);
#endif
