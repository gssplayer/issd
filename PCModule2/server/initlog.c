#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include "issd_server.h"

void init_log()
{
	// close stdin stdout, stderr
	close(0);
	close(1);
	close(2);
	
	// redirect stdin, stdout, stderr
	int infd = open("/dev/null", O_RDWR);
	int outfd = open(LOG_FILENAME, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0666);	    
	int errfd = open(LOG_ERR_FILENAME, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0666);
	if(infd == -1 || outfd == -1 || errfd == -1)
		exit(-1);
	
/*	char strfd[10];
	snprintf(strfd, 10, "%d", outfd);
	write(outfd, strfd, strlen(strfd));
	snprintf(strfd, 10, "%d", errfd);
	write(errfd, strfd, strlen(strfd));*/
}
