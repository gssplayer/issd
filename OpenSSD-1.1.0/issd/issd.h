#ifndef ISSD_H_
#define ISSD_H_

#include "jasmine.h"

#define ISSD_MAX_SEQ_BLOCK 30

// NOTE: sizeof(afCommand_t) <= 512
typedef struct afCommand {
	int opType; 
#define ISSD_BACKUP 0

	UINT32 fileSize; // WRITE_BUFFER:read file length, READ_BUFFER:write file size

	UINT32 nr_rd_bios; // number of read sequence blocks, note: nr_rd_bios < ISSD_MAX_SEQ_BLOCK
	UINT32 rd_bios[ISSD_MAX_SEQ_BLOCK];
	UINT32 rd_bios_len[ISSD_MAX_SEQ_BLOCK];
	
	UINT32 nr_wr_bios; // number of write sequence blocks, note: nr_wr_bios < ISSD_MAX_SEQ_BLOCK
	UINT32 wr_bios[ISSD_MAX_SEQ_BLOCK];
	UINT32 wr_bios_len[ISSD_MAX_SEQ_BLOCK];
	
	int isOK; // read back by host to indicate issd_process_cmd successful.
} afCommand_t;

void issd_process_cmd(void);

int do_issd_backup(void);

void issd_output(UINT32 buf_addr, int length);

extern int issd_read_sectors(UINT32 buf_addr, UINT32 lba, UINT32 num_sectors);

extern int issd_write_sectors(UINT32  buf_addr, UINT32 lba, UINT32 num_sectors);


#endif /*ISSD_H_*/
