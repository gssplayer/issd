#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#define BUF_SIZE 512

static int scansysdir(char* namebuf, char *sysdir, dev_t dev)
{
	char* dirtailptr = strchr(sysdir, '\0');
	
	DIR *dir;
	int done = 0;
	
	struct dirent *de;
	char *systail;
	FILE *sysdev;
	unsigned long ma, mi;
	char *ep;
	ssize_t rd;

	dir = opendir(sysdir);
	if(!dir)
	{
		perror("opendir");
		return 0;
	}

	*dirtailptr++ = '/';

	while(!done && (de = readdir(dir))){
		/* Assume if we see a dot-name in sysfs it's special */
//		printf("%s:%d\n", de->d_name, de->d_type);
		if(de->d_name[0] == '.')
			continue;
		
		if(de->d_type != DT_UNKNOWN && de->d_type != DT_DIR && de->d_type != DT_LNK)
			continue;
		if(strlen(de->d_name) >=
			(BUF_SIZE - 64) - (dirtailptr - sysdir))
			continue;

		strcpy(dirtailptr, de->d_name);
		systail = strchr(sysdir, '\0');
		
//		printf("current dir = %s\n", sysdir);		

		strcpy(systail, "/dev");
		sysdev = fopen(sysdir, "r");
		if(!sysdev)
			continue;

		/* Abusing the namebuf as tempory storage here. */
		rd = fread(namebuf, 1, BUF_SIZE, sysdev);
		namebuf[rd] = '\0';

		fclose(sysdev);
	
		ma = strtoul(namebuf, &ep, 10);
		if(ma != major(dev) || *ep != ':')
			continue;
		
		mi = strtoul(ep + 1, &ep, 10);
		if(*ep != '\n')
			continue;

		if(mi == minor(dev)){
			/* Found it! */
			strcpy(namebuf, de->d_name);
			done = 1;
		} else {
			/* we have a major number match, scan for partitions */
			*systail = '\0';
			done = scansysdir(namebuf, sysdir, dev);
		}
	}
	
	closedir(dir);
	return done;
}

char *bdevname(dev_t dev, char* buf)
{
	char sysdir[BUF_SIZE];

	strcpy(sysdir, "/sys/block");
	
	if(!scansysdir(buf, sysdir, dev))
		return NULL;

	return buf;	
}

