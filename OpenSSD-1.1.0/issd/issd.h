#ifndef ISSD_H_
#define ISSD_H_

#include "jasmine.h"

#define ISSD_MAX_OP_FILES 3

typedef struct lbaExt{
	UINT32 lba;
	UINT32 len;
} lbaExt_t;

#define ISSD_BACKUP 0

typedef struct afCommand{
	int num_packet;
	int opType;
	int isOK;
	
	// file extents
	int num_files; // num_files <= ISSD_MAX_OP_FILES
	UINT32 fileSize[ISSD_MAX_OP_FILES];
	UINT32 file_ext_offset[ISSD_MAX_OP_FILES + 1];
	lbaExt_t fileExtArr[];
} afCommand_t;


void issd_process_cmd(void);

int do_issd_backup(void);

void issd_output(UINT32 buf_addr, int length);

extern int issd_read_sectors(UINT32 buf_addr, UINT32 lba, UINT32 num_sectors);

extern int issd_write_sectors(UINT32  buf_addr, UINT32 lba, UINT32 num_sectors);


#endif /*ISSD_H_*/
