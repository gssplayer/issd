// issd computing
#include "jasmine.h"
#include "issd.h"


static afCommand_t* afCmd;
typedef struct wr_sector_pointer {
	int index, offset;
} wr_sector_pointer_t;

static wr_sector_pointer_t curSector;
void issd_process_cmd(void)
{
	afCmd = (afCommand_t *)ISSD_CMD_BUFFER_ADDR;
	
	uart_printf("opType: %d\n", read_dram_32(&(afCmd->opType)));
	//uart_printf("rlength: %d\n", read_dram_32(afCmd->rlength));
	//uart_printf("nr_rd_bios: %d\n", read_dram_32(&(afCmd->nr_rd_bios)));
	//uart_printf("wlength: %d\n", read_dram_32(afCmd->wlength));
	//uart_printf("nr_wr_bios: %d\n", read_dram_32(&(afCmd->nr_wr_bios)));
	//uart_printf("input fileSize = %d\n", read_dram_32(&(afCmd->fileSize)));
	uart_printf("in:isOK = %d\n", read_dram_32(&(afCmd->isOK)));
	
	int opType = read_dram_32(&(afCmd->opType));
	switch(opType)
	{
	case ISSD_BACKUP:
		do_issd_backup();
		break;
		
	default:
		uart_printf("invalid command!\n");
		break;
	}
}

int do_issd_backup(void)
{
	uart_print("In do_issd_backup");
	
/******************************************************************
*** just for test communication with the host**********************
*** int cnt = 10000;
	while(cnt > 0) cnt--;
	
	int fileSize = read_dram_32(&(afCmd->fileSize));
	write_dram_32(&(afCmd->fileSize), fileSize * 2);
	write_dram_32(&(afCmd->isOK), 1);
	
	uart_printf("output fileSize = %d\n", read_dram_32(&(afCmd->fileSize)));
	uart_printf("out:isOK = %d\n", read_dram_32(&(afCmd->isOK)));
********************************************************************/
	
	int i;
	int ret = 0;
	
	UINT32 buf_addr;
	
	UINT32 read_file_offset_start = 0;
	UINT32 read_file_offset_end = 0;
	//UINT32 buf_addr;
	
	// int nr_rd_bios = read_dram_32(&(afCmd->file_ext_offset[1])) - read_dram_32(&(afCmd->file_ext_offset[0]));	
	// int nr_wr_bios = read_dram_32(&(afCmd->file_ext_offset[2])) - read_dram_32(&(afCmd->file_ext_offset[1]));	
	// int fileSize = read_dram_32(&(afCmd->fileSize[0]));
	
	int rd_bio_offset = read_dram_32(&(afCmd->file_ext_offset[0]));
	int wr_bio_offset = read_dram_32(&(afCmd->file_ext_offset[1]));
	int total_bios = read_dram_32(&(afCmd->file_ext_offset[2]));

	int nr_rd_bios = wr_bio_offset - rd_bio_offset;
	int nr_wr_bios = total_bios - wr_bio_offset;	
	int fileSize = read_dram_32(&(afCmd->fileSize[0]));
	
	// int nr_rd_bios = read_dram_32(&(afCmd->nr_rd_bios));
	// int nr_wr_bios = read_dram_32(&(afCmd->nr_wr_bios));
	// int fileSize = read_dram_32(&(afCmd->fileSize));
	curSector.index = 0;
	curSector.offset = 0;
	
	for(i = 0; i < nr_rd_bios; i++)
	{
		int lba = read_dram_32(&((afCmd->fileExtArr[rd_bio_offset + i]).lba));
		uart_printf("lba start:%d", lba);
		int num_sectors = read_dram_32(&((afCmd->fileExtArr[rd_bio_offset + i]).len));
		uart_printf("lba len:%d", num_sectors);
		
		while(num_sectors != 0)
		{
			int start_lba_page = lba / SECTORS_PER_PAGE;
			int end_lba_page = (lba + num_sectors + SECTORS_PER_PAGE - 1) / SECTORS_PER_PAGE;
			int need_pages = end_lba_page - start_lba_page;
			
			int start_sector_offset = lba % SECTORS_PER_PAGE;
			int rd_sectors;
			if(need_pages > ISSD_CNT_PAGES)
			{
				rd_sectors = ISSD_CNT_PAGES * SECTORS_PER_PAGE - start_sector_offset;
			}
			else
			{
				rd_sectors = num_sectors;
			}
			
			
			ret = issd_read_sectors(ISSD_COMPUTING_ADDR, lba, rd_sectors);
			if(ret < 0)
			{
				uart_print("issd_read_sectors failed!\n");
				return -1;
			}
			
			lba += rd_sectors;
			num_sectors -= rd_sectors;
			
			if(read_file_offset_start + rd_sectors * BYTES_PER_SECTOR >= fileSize)
			{
				read_file_offset_end = fileSize;
			}
			else
			{
				read_file_offset_end = read_file_offset_start + (rd_sectors * BYTES_PER_SECTOR);
			}
			
			//issd_output(ISSD_COMPUTING_ADDR + start_sector_offset * BYTES_PER_SECTOR, (read_file_offset_end - read_file_offset_start));
			
			int wr_sectors = (read_file_offset_end - read_file_offset_start + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR;
			
			buf_addr = ISSD_COMPUTING_ADDR + start_sector_offset * BYTES_PER_SECTOR;
			
			while(wr_sectors != 0)
			{
				ASSERT(curSector.index < nr_wr_bios);
				int curBioLen = read_dram_32(&((afCmd->fileExtArr[wr_bio_offset + curSector.index]).len));
				
				int left_wr_sectors = curBioLen - curSector.offset;
				
				int send_wr_sectors;
				int is_wr_seqblk_finish = 0;
				
				if(wr_sectors >= left_wr_sectors)
				{
					send_wr_sectors = left_wr_sectors;
					is_wr_seqblk_finish = 1;
				}
				else
				{
					send_wr_sectors = wr_sectors;
				}
				
				int curBioLba = read_dram_32(&((afCmd->fileExtArr[wr_bio_offset + curSector.index]).lba));
				int wr_lba = curBioLba + curSector.offset;
				
				uart_printf("wr_lba = %d", wr_lba);
				uart_printf("send_wr_sectors = %d", send_wr_sectors);
				
				ret = issd_write_sectors(buf_addr, wr_lba, send_wr_sectors);
				if(ret < 0)
				{
					uart_print("issd_write_sectors failed!\n");
					return -1;
				}
				
				if(is_wr_seqblk_finish)
				{
					curSector.index++;
					curSector.offset = 0;
				}
				else
				{
					curSector.offset += send_wr_sectors;
				}
				
				wr_sectors -= send_wr_sectors;
				buf_addr += send_wr_sectors * BYTES_PER_SECTOR;
			}
			
			read_file_offset_start = read_file_offset_end;
			
		}
	}
	
	// set successful flag
	write_dram_32(&(afCmd->isOK), 1);
	return 0;
}

void issd_output(UINT32 buf_addr, int length)
{
	//ASSERT(buf_addr == ISSD_COMPUTING_ADDR);
	uart_printf("buf len:%d", length);
	UINT32 paddr = buf_addr;
	UINT32 paddr_end = buf_addr + length;
	while(paddr < paddr_end)
	{
		UINT8 ostr = read_dram_8(paddr);
		
		/*if(ostr == 0)
			uart_print("lchar = 0");
		else
			uart_print("lchar != 0");*/
		uart_txbyte(ostr);
		
		paddr++;
	}
}

