// issd_compute implementation
#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <linux/hdreg.h>
#include <math.h>
#include <stdarg.h>

#include "issd.h"

#define MAX_FILENAME_LEN 1024

#define MAX_FILE_BLOCK_LEN 65536 // max_file_size  = 2^16 * 2^12 = 256M
//#define FILE_BLKSIZE 4096
//#define FILE_BLKBITS 12
//#define PART_FIRST_SECTOR 2048 // partition first secor no.

#define FALLOC_FL_READ_THROUGH 0x08

struct {
	char va_ifname[MAX_FILENAME_LEN];
} ISSD_MERGE_va;

int max_fileExt_num;


int get_fileSize(int fd)
{
	struct stat buf;
	int ret;

	ret = fstat(fd, &buf);
	if(ret < 0)
	{
		perror("stat");
		return -1;
	}

	return buf.st_size;
}

/*
 * get_nr_blocks - return the number of logical blocks
 * consumed by the file associated with fd.
 */
int get_nr_blocks(int fd, int blksize)
{
	int fileSize = get_fileSize(fd);

	int nr_blocks = (fileSize + blksize - 1) / blksize;
	return nr_blocks;
}

/*
 * get_block -  for the file associated with the given fd, returns
 * the physical block mapping to logical block.
 */
sector_t get_block(int fd, sector_t logical_block)
{
	int ret;

	ret = ioctl(fd, FIBMAP, &logical_block);
	if(ret < 0)
	{
		perror("iotcl");
		return -1;
	}
	
	return logical_block;
}

int extractBlocks(int fd, sector_t blockArr[], int n, int blksize)
{
	int nr_blocks, i;
	
	memset(blockArr, -1, sizeof(sector_t) * n);
	
	nr_blocks = get_nr_blocks(fd, blksize);
	if(nr_blocks < 0) return -1;

	if(nr_blocks > n)
	{
		fprintf(stderr, "file too large\n");
		return -1;
	}

	for(i = 0; i < nr_blocks; i++)
	{
		sector_t phys_block;
		phys_block = get_block(fd, (sector_t)i);
		if(phys_block  < 0)
		{
			fprintf(stderr, "get block failed\n");
			return -1;
		}
		blockArr[i] = phys_block;
	}

	return 0;
}

int postProcess(afCommand_t *pCmd, int curFileIdx, sector_t *blocks, UINT32 nr_blocks, int blkbits, sector_t part_first_sector)
{
	int start =0, end = 0;
	int cnt = 0;
	int offset = pCmd->file_ext_offset[curFileIdx];
	
	while(end < nr_blocks)
	{
		while(end < nr_blocks && (end == start || blocks[end] == blocks[end - 1] + 1))
			end++;

		pCmd->fileExtArr[offset + cnt].lba = part_first_sector +(blocks[start] << (blkbits - 9));
		printf("lba start:%d\n", pCmd->fileExtArr[offset + cnt].lba);
		pCmd->fileExtArr[offset + cnt].len = (end - start) << (blkbits - 9);
		printf("lba len:%d\n", pCmd->fileExtArr[offset + cnt].len);
		cnt++;
		start = end;	
		
		if(offset + cnt >= max_fileExt_num)
		{
			fprintf(stderr, "too many file extents!\n");
			return -1;
		}	
	}
	
	return cnt;
}

int my_fallocate(int fd, int offset, int len)
{
	// ASSUMPTION: awalys start from zero
	assert(offset == 0);
	printf("In my_fallocate\n");

//	int nfileSize = offset + len;
	int blksize = 512;

	char *buffer = NULL;
	int ret = posix_memalign(&buffer, blksize, blksize);
	if(ret)
	{
		perror("posix_memalign");
		return -1;
	}

	/* write something to erery block */
	for(; offset < len; offset += SECTOR_SIZE)
	{
			int leftSize = SECTOR_SIZE;
			int nwrite;
			while(leftSize > 0)
			{
				nwrite = write(fd, buffer, leftSize);
				if(nwrite < 0)
					return -1;
				leftSize -= nwrite;
			}

	}
	free(buffer);
/*	ret = ftruncate(fd, nfileSize);
	if(ret)
	{
		perror("ftruncate");
		return -1;
	}*/

	printf("new fileSize = %d\n", get_fileSize(fd));	
	return 0;
}

// successful: return number of lba extents;
// failed: return -1;
static int getLbaExts(afCommand_t *pCmd, int curFileIdx, char* fname, issd_env_t *env)
{
	const int blkbits = env->fs_blkbits;
	const int blksize = env->fs_blksize;
	const sector_t part_first_sector = env->part_start;

	int ret;
	int fd;
	fd = open(fname, O_RDONLY);
	if(fd < 0)
	{
		perror("open");
		goto failed;
	}

	if((pCmd->fileSize[curFileIdx] = get_fileSize(fd)) < 0)
		goto after_open_failed;

	sector_t blockList[MAX_FILE_BLOCK_LEN];
	ret = extractBlocks(fd, blockList, MAX_FILE_BLOCK_LEN, blksize);
	if(ret < 0)
	{
		fprintf(stderr, "extractBlocks failed!\n");
		goto after_open_failed;
	}

	int nr_blocks = get_nr_blocks(fd, blksize);
	
	int segs = postProcess(pCmd, curFileIdx, blockList, nr_blocks, blkbits, part_first_sector);
	if(segs < 0)
	{
		goto after_open_failed;
	}
	
	close(fd);
	return segs;

	after_open_failed:
		close(fd);
	failed:
		return -1;
}

int issd_createCmd(afCommand_t * pCmd, char *ifname, char* ofname, issd_env_t * env)
{

	const int blkbits = env->fs_blkbits;
	const int blksize = env->fs_blksize;
	const sector_t part_first_sector = env->part_start;
	
	pCmd->num_files = 2;
	
	int curFileIdx = 0;

	int segs = getLbaExts(pCmd, curFileIdx, ifname, env);	
	if(segs < 0)
	{
		fprintf(stderr, "getLbaExts failed!\n");
		goto failed;
	}	
	
	curFileIdx++;
	pCmd->file_ext_offset[curFileIdx] = pCmd->file_ext_offset[curFileIdx - 1] + segs;
	
	switch(pCmd->opType)
	{
		case ISSD_FILECOPY:
			break;
		case ISSD_MERGE:
		{
			pCmd->num_files++;
			
			segs = getLbaExts(pCmd, curFileIdx, ISSD_MERGE_va.va_ifname, env);
			if(segs < 0)
			{
				fprintf(stderr, "line %d: getLbaExts failed!\n", __LINE__);
				goto failed;
			}
			
			curFileIdx++;
			pCmd->file_ext_offset[curFileIdx] = pCmd->file_ext_offset[curFileIdx - 1] + segs;
			
			break;
		}
		default:
			break;
	}
	
	int wlength;	
	switch(pCmd->opType)
	{
		case ISSD_FILECOPY:
		{
			wlength = pCmd->fileSize[0];
			break;
		}
		case ISSD_MERGE:
		{
			wlength = pCmd->fileSize[0] + pCmd->fileSize[1] + blksize;
			break;
		}
		default:
		{
			wlength = pCmd->fileSize[0];
			break;
		}
	}	

	int wfd;
	wfd = open(ofname, O_RDWR | O_CREAT | O_DIRECT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(wfd < 0)
	{
		perror("create");
		goto failed;	
	}

#if ISSD_SUPPORT_FALLOCATE
/*	if(posix_fallocate(wfd, 0, wlength))
	{
		perror("posix_fallocate");
		goto after_open_wfile_failed;	
	}*/

	if(fallocate(wfd, FALLOC_FL_READ_THROUGH, 0, wlength))
	{
		perror("fallocate");
		goto after_open_wfile_failed;
	}
#else
	if(my_fallocate(wfd, 0, wlength))
	{
		perror("my_fallocate");
		goto after_open_wfile_failed;
	}		
#endif
	close(wfd);
	
	segs = getLbaExts(pCmd, curFileIdx, ofname, env);
	if(segs < 0)
	{
		fprintf(stderr, "line %d: getLbaExts failed!\n", __LINE__);
		goto failed;
	}

	curFileIdx++;
	pCmd->file_ext_offset[curFileIdx] = pCmd->file_ext_offset[curFileIdx - 1] + segs;

	pCmd->isOK = 0;	
	return 0;
		
after_open_wfile_failed:
	close(wfd);
	unlink(ofname);
failed:	return -1;
}


int issd_compute(int opType, char *ifname, char* ofname, char* devname, ...)
{
	assert(ifname != NULL);
	assert(ofname != NULL);

	va_list arg_ptr;
	
	va_start(arg_ptr, devname);

	switch(opType)
	{
		case ISSD_FILECOPY:
			break;
		case ISSD_MERGE:
		{
			char *ifname2 = va_arg(arg_ptr, char*);
			strncpy(ISSD_MERGE_va.va_ifname, ifname2, MAX_FILENAME_LEN);
			break;
		}
		default:
			break;
	}	

	issd_env_t ienv;
	snprintf(ienv.devname, 20, "%s", devname);
	// get part first no.
	struct hd_geometry geom;
	int dev_fd = open(devname, O_RDONLY);
	if(dev_fd != -1 && !ioctl(dev_fd, HDIO_GETGEO, &geom))
	{
		ienv.part_start = geom.start;
	}
	else
	{
		if(dev_fd != -1)
			close(dev_fd);

		fprintf(stderr, "open/ioctl failed\n");
		return -1;
	}
	fprintf(stdout, "%s start sector offset: %d\n", devname, ienv.part_start);
	close(dev_fd);

	// get fs blksize and blkbit
	struct stat sb;
	if(stat(ifname, &sb))
	{
		perror("stat");
		return -1;
	}

	ienv.fs_blksize = sb.st_blksize;
	ienv.fs_blkbits =(int) (log(sb.st_blksize) / log(2));

	afCommand_t *afCmd = NULL;
	afCmd = (afCommand_t *)malloc(MAX_AFCMD_PACKET_SIZE);
	if(!afCmd)
	{
		fprintf(stderr, "%s at line %d:malloc failed!\n", __FUNCTION__, __LINE__);
		return -1;		
	}

	max_fileExt_num = (MAX_AFCMD_PACKET_SIZE - MAX_AFCMD_MAINPARA_SIZE) / sizeof(lbaExt_t);  	
	memset(afCmd, 0, MAX_AFCMD_PACKET_SIZE);
	afCmd->opType = opType;

	int ret = 0;
	ret = issd_createCmd(afCmd, ifname, ofname, &ienv);
	if(ret < 0)
	{
		fprintf(stderr, "createCmd failed\n");
		goto failed;
	}

	int afCmd_size = sizeof(afCommand_t) + sizeof(lbaExt_t) * (afCmd->file_ext_offset[afCmd->num_files]);
	
	ret = issd_sendCmd(afCmd, afCmd_size, devname);
	if(ret < 0)
	{
		fprintf(stderr, "sendCmd failed!\n");
		goto failed;
	}

	if(issd_waitFinish(afCmd, devname) < 0)
		goto failed;

	// modify wFizeSize
	ret = truncate(ofname, afCmd->fileSize[afCmd->num_files - 1]);
	if(ret)
	{
		perror("truncate");
		goto failed;
	}
	
	free(afCmd);
	return 0;	
	
	failed:
		free(afCmd);
		unlink(ofname);
		return -1;
}

int issd_sendCmd(afCommand_t *pCmd, int cmdSize, char *devname)
{
	return sata_writeBuffer((void*)pCmd, cmdSize, devname);
}

int issd_waitFinish(afCommand_t *rCmd, char* devname)
{

	afCommand_t *pCmd = NULL;
	unsigned char* rbuffer = malloc(MAX_AFCMD_MAINPARA_SIZE);

	if(rbuffer == NULL)
	{
		perror("malloc");
		return -1;
	}

	memset(rbuffer, 0, MAX_AFCMD_MAINPARA_SIZE);
	pCmd = (afCommand_t *)rbuffer;
	while(1)
	{
		if(sata_readBuffer((void*)rbuffer, devname) < 0)
		{
			free(rbuffer);
			return -1;
		}
#if TEST_READ_BUFFER
	printf("pcmd->isOK = %d\n", pCmd->isOK);
	printf("pcmd->opType = %d\n", pCmd->opType);

#endif
		if(pCmd->isOK)
			break;

		sleep(1); // wait 1 second
	}
	
	memcpy(rCmd, rbuffer, sizeof(afCommand_t));
	free(rbuffer);	
	return 0;
}

