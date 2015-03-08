//
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "issd_lib.h"

int main(int argc, char* argv[])
{
	if(argc <= 2)
	{
		printf("usage:./issdOp ifname ofname");
		return 0;
	}
	
	issd_compute_init();
	
	issd_compute(argv[1], argv[2], 0);	

	issd_compute_destroy();

	return 0;
}
