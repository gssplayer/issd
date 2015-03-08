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

#include "issd.h"

#define MAX_FILE_BLOCK_LEN 65536 // max_file_size  = 2^16 * 2^12 = 256M
//#define FILE_BLKSIZE 4096
//#define FILE_BLKBITS 12
//#define PART_FIRST_SECTOR 2048 // partition first secor no.

#define FALLOC_FL_READ_THROUGH 0x08


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

int postProcess(sector_t *Bios, UINT32 *BioLen, sector_t *blocks, UINT32 nr_blocks, int blkbits, sector_t part_first_sector)
{
	int start =0, end = 0;
	int cnt = 0;
	
	while(end < nr_blocks)
	{
		while(end < nr_blocks && (end == start || blocks[end] == blocks[end - 1] + 1))
			end++;

		Bios[cnt] = part_first_sector +(blocks[start] << (blkbits - 9));
		printf("lba start:%d\n", Bios[cnt]);
		BioLen[cnt] = (end - start) << (blkbits - 9);
		printf("lba len:%d\n", BioLen[cnt]);
		cnt++;
		start = end;

		if(cnt >= ISSD_MAX_SEQ_BLOCK)
		{
			fprintf(stderr, "too many seq block!\n");
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

int issd_createCmd(afCommand_t * pCmd, char *ifname, char* ofname, issd_env_t * env)
{
	int ret = 0;

	const int blkbits = env->fs_blkbits;
	const int blksize = env->fs_blksize;
	const sector_t part_first_sector = env->part_start;

	int rfd, wfd;
	
	rfd = open(ifname, O_RDONLY);
	if(rfd < 0)
	{
		perror("open");
		goto failed;
	}

	if((pCmd->fileSize = get_fileSize(rfd)) < 0)
		goto after_open_rfile_failed;
	
	
	sector_t rblocks[MAX_FILE_BLOCK_LEN];
	ret = extractBlocks(rfd, rblocks, MAX_FILE_BLOCK_LEN, blksize);
	if(ret < 0)
	{
		fprintf(stderr, "extractBlocks failed\n");
		goto after_open_rfile_failed;
	}

	int rd_nr_blocks = get_nr_blocks(rfd, blksize);
	
	int segs = postProcess(pCmd->rBios, pCmd->rBioLen, rblocks, rd_nr_blocks, blkbits, part_first_sector);
	if(segs < 0)
	{
	//	fprintf(stderr, "too many seq blocks.\n");
		goto after_open_rfile_failed;
	}	

	pCmd->nr_rd_bios = segs;
	close(rfd);
	
	UINT32 wlength = pCmd->fileSize; // assumption:ofileSize <= ifileSize

	wfd = open(ofname, O_RDWR | O_CREAT | O_DIRECT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(wfd < 0)
	{
		perror("create");
		goto after_open_rfile_failed;	
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
	
	sector_t wblocks[MAX_FILE_BLOCK_LEN];
	ret = extractBlocks(wfd, wblocks, MAX_FILE_BLOCK_LEN, blksize);
	if(ret < 0)
	{
		fprintf(stderr, "extract wr block failed\n");
		goto after_open_wfile_failed;	
	}
	
	int wr_nr_blocks = get_nr_blocks(wfd, blksize);
	segs = postProcess(pCmd->wBios, pCmd->wBioLen, wblocks, wr_nr_blocks, blkbits, part_first_sector);
	if(segs < 0)
	{
		fprintf(stderr, "too many seq block.\n");
		goto after_open_wfile_failed;
	}
	pCmd->nr_wr_bios = segs;
	close(wfd);
	pCmd->isOK = 0;	
	return 0;
		
after_open_wfile_failed:
	close(wfd);
	unlink(ofname);
after_open_rfile_failed:
	close(rfd);
failed:	return -1;
}


int issd_compute(int opType, char *ifname, char* ofname, char* devname)
{
	assert(ifname != NULL);
	assert(ofname != NULL);

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

	afCommand_t afCmd;
	afCmd.opType = opType;

	int ret = 0;
	ret = issd_createCmd(&afCmd, ifname, ofname, &ienv);
	if(ret < 0)
	{
		fprintf(stderr, "createCmd failed\n");
		goto failed;
	}
	
#if TEST_EXTRACT_LBA
{
	int testRfd = open("testRfile", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IROTH);
	if(testRfd < 0)
	{
		perror("open");
		goto failed;
	}
	
	int nr_rd_bios = afCmd.nr_rd_bios, i, j;
	int fileSize = afCmd.fileSize;
	char buffer[SECTOR_SIZE];
	int nread;
	int leftSize;
	
	int devFd = open("/dev/sdb", O_RDWR);
	if(devFd < 0)
	{
		perror("open");
		goto failed;
	}

	for(i = 0; i < nr_rd_bios -1; i++)
	{
		int lba = afCmd.rBios[i];
		if(lseek(devFd, lba * SECTOR_SIZE, SEEK_SET) < 0)
		{
			perror("lseek");
			goto failed;
		}

		for(j = 0; j < afCmd.rBioLen[i]; j++)
		{
			leftSize = SECTOR_SIZE;
			while(leftSize > 0 && (nread = read(devFd, buffer, leftSize)) > 0)
			{
				write(testRfd, buffer, nread);
				leftSize -= nread;
			}
			fileSize -= SECTOR_SIZE;
		}
	}

	int lba = afCmd.rBios[i];
	if(lseek(devFd, lba * SECTOR_SIZE, SEEK_SET) < 0)
	{
		perror("lseek");
		goto failed;
	}
	
	for(j = 0; fileSize > 0 && j < afCmd.rBioLen[i]; j++)
	{
		leftSize = fileSize > SECTOR_SIZE ? SECTOR_SIZE : fileSize;
		while(leftSize >0 && (nread = read(devFd, buffer, leftSize)) > 0)
		{
			write(testRfd, buffer, nread);
			leftSize -= nread;
		}
		
		fileSize -= SECTOR_SIZE;
	}

	close(testRfd);
	close(devFd);
	return 0;
}
#endif
	
/*	int sataFd = open(DEVICE_NAME, O_RDWR);
	if(sataFd < 0)
	{
		perror("open");
		goto failed;
	}*/

	ret = issd_sendCmd(&afCmd, devname);
	if(ret < 0)
	{
		fprintf(stderr, "sendCmd failed!\n");
		goto failed;
	}

	if(issd_waitFinish(&afCmd, devname) < 0)
		goto failed;

	// modify wFizeSize
	ret = truncate(ofname, afCmd.fileSize);
	if(ret)
	{
		perror("truncate");
		goto failed;
	}

//	close(sataFd);
#if TEST_OUT_FILE
{
	int testOutfd = open("testOutfile", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IROTH);
	if(testOutfd < 0)
	{
		perror("open");
		goto failed;
	}
	
	int nr_wr_bios = afCmd.nr_wr_bios, i, j;
	int ofileSize = afCmd.fileSize;
	char buffer[SECTOR_SIZE];
	int nread;
	int leftSize;
	
	int devFd = open("/dev/sdb", O_RDWR);
	if(devFd < 0)
	{
		perror("open");
		goto failed;
	}

	for(i = 0; i < nr_wr_bios -1; i++)
	{
		int lba = afCmd.wBios[i];
		if(lseek(devFd, lba * SECTOR_SIZE, SEEK_SET) < 0)
		{
			perror("lseek");
			goto failed;
		}

		for(j = 0; j < afCmd.wBioLen[i]; j++)
		{
			leftSize = SECTOR_SIZE;
			while(leftSize > 0 && (nread = read(devFd, buffer, leftSize)) > 0)
			{
				write(testOutfd, buffer, nread);
				leftSize -= nread;
			}
			ofileSize -= SECTOR_SIZE;
		}
	}

	int lba = afCmd.wBios[i];
	if(lseek(devFd, lba * SECTOR_SIZE, SEEK_SET) < 0)
	{
		perror("lseek");
		goto failed;
	}
	
	for(j = 0; ofileSize > 0 && j < afCmd.wBioLen[i]; j++)
	{
		leftSize = ofileSize > SECTOR_SIZE ? SECTOR_SIZE : ofileSize;
		while(leftSize >0 && (nread = read(devFd, buffer, leftSize)) > 0)
		{
			write(testOutfd, buffer, nread);
			leftSize -= nread;
		}
		
		ofileSize -= SECTOR_SIZE;
	}

	close(testOutfd);
	close(devFd);
	return 0;
}
#endif

	return 0;	
	
	failed:
		unlink(ofname);
		return -1;
}

int issd_sendCmd(afCommand_t *pCmd, char *devname)
{
	return sata_writeBuffer((void*)pCmd, sizeof(afCommand_t), devname);
}

int issd_waitFinish(afCommand_t *rCmd, char* devname)
{

	afCommand_t *pCmd = NULL;
	unsigned char* rbuffer = malloc(SECTOR_SIZE);

	if(rbuffer == NULL)
	{
		perror("malloc");
		return -1;
	}

	memset(rbuffer, 0, SECTOR_SIZE);
	pCmd = (afCommand_t *)rbuffer;
	while(1)
	{
		if(sata_readBuffer((void*)rbuffer, devname) < 0)
		{
			free(rbuffer);
			return -1;
		}
#if TEST_READ_BUFFER
	printf("pcmd->isOK = %d, pcmd->fileSize = %d\n", pCmd->isOK, pCmd->fileSize);
	printf("pcmd->opType = %d\n", pCmd->opType);

#endif
		if(pCmd->isOK)
			break;

		sleep(1); // wait 1 second
	}
	
	memcpy(pCmd, rbuffer, sizeof(afCommand_t));
	free(rbuffer);	
	return 0;
}

