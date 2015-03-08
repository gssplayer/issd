#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <scsi/sg.h>
#include <assert.h>
//#include <scsi/scsi.h>

#include "issd.h"

#define RWB_MODE_DATA 2
//#define SECTOR_SIZE 512

#define READ_BUFFER 0xe4
#define WRITE_BUFFER 0xe8
int sata_writeBuffer(void* pCmd, int bufsize, char* devname)
{
	int sg_fd;

	sg_fd = open(devname, O_RDWR );
	if(sg_fd < 0)
	{
		perror("open device error");
		return -1;
	}	


	unsigned char cmdp[16];
	unsigned char sbp[32];
	sg_io_hdr_t io_hdr;
	unsigned char * data;
	int size = SECTOR_SIZE;
	
	data = malloc(size);
	if(data == NULL) 
	{
		close(sg_fd);
		return -1;
	}

	memset(&cmdp, 0, 16);
	memset(&sbp, 0, 32);
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	memset(data, 0, size);
	memcpy(data, (void*)pCmd, bufsize);
	
	
	io_hdr.interface_id = 'S';
	io_hdr.cmdp = cmdp;
	io_hdr.cmd_len = sizeof(cmdp);
	io_hdr.dxferp = data;
	io_hdr.dxfer_len = size;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.sbp = sbp;
	io_hdr.mx_sb_len =  sizeof(sbp);
	io_hdr.timeout = 12000; // 12 seconds

	cmdp[0] = 0x85; // pass-through ATA16 command (no translation)
	cmdp[1] = (5 << 1); // data-out
	cmdp[2] = 0x26;
	cmdp[14] = WRITE_BUFFER;
		
	

	if(ioctl(sg_fd, SG_IO, &io_hdr))
	{
		close(sg_fd);
		perror("SG_IO WRITE_BUFFE data error");
		return -1;
	}
	close(sg_fd);
	return 0;
}


int sata_readBuffer(void* rbuffer, char* devname)
{
	assert(rbuffer != NULL);
	int sg_fd;

	sg_fd = open(DEVICE_NAME, O_RDWR );
	if(sg_fd < 0)
	{
		perror("open device error");
		return -1;
	}	


	unsigned char cmdp[16];
	unsigned char sbp[32];
	sg_io_hdr_t io_hdr;
	
	memset(&cmdp, 0, 16);
	memset(&sbp, 0, 32);
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	
	
	io_hdr.interface_id = 'S';
	io_hdr.cmdp = cmdp;
	io_hdr.cmd_len = sizeof(cmdp);
	io_hdr.dxferp = rbuffer;
	io_hdr.dxfer_len = SECTOR_SIZE;
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.sbp = sbp;
	io_hdr.mx_sb_len =  sizeof(sbp);
	io_hdr.timeout = 12000; // 12 seconds

	cmdp[0] = 0x85; // pass-through ATA16 command (no translation)
	cmdp[1] = (4 << 1); // data-in
	cmdp[2] = 0x2e;
	cmdp[14] = READ_BUFFER;
		
	

	if(ioctl(sg_fd, SG_IO, &io_hdr))
	{
		close(sg_fd);
		perror("SG_IO READ_BUFFE data error");
		return -1;
	}
	close(sg_fd);
	return 0;
return 0;
}
