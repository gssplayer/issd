// issd server main function
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "issd_server.h"
#define _GNU_SOURCE
#include <getopt.h>


#define no_argument 0
#define required_argument 1

/**
 * Create this struct to store options(in short and long form)
 * and help in the same variable. The struct option array, which 
 * is needed for getopt_long is build dynamically by build _option_table();
 * We can add options here there  isn't problems with getopt_long().
 */
struct{
	char *id;
	char has_arg;
	int *flag;
	int val;
	char* help;
}extd_options[] = {
{"help", no_argument, NULL, 'h', "display this help message"},
{"nItems", required_argument, NULL, 'n', "set the number of request items in shared memory"},
{"nDevice", required_argument, NULL, 'd', "set the number of ssd device"},
{"version", no_argument, NULL, 'v', "display the version"},
{NULL, 0, NULL, 0, NULL}
};

/*
 * String to use with getopt or getopt_long
 * 
 * if you update extd_options[] you must also add option in short form
 * to this string
 */
static const char* short_option_string = "hn:d:v";

static void
usage()
{
	char *s= "Usage: issd_compute_server [option]\n";
	puts(s);
}

static void
print_help_msg()
{
	usage();
	unsigned int i;
	for(i = 0; extd_options[i].id != NULL; i++)
	{
		printf("-%c, --%-23.23s %s\n", extd_options[i].val, extd_options[i].id, extd_options[i].help);
	}
}

static void
print_version()
{
	const char *s = "version 0.1\n";
	puts(s);
}

/**
 * build a struct option array from exted_options table
 * \return a pointer to NULL terminated struct option array
 * allocated with malloc */
static struct option *
build_option_table()
{
	unsigned int i, len = sizeof(extd_options)/ sizeof(extd_options[0]);
	struct option *option = malloc(sizeof(struct option) * len);
	if(option == NULL)
	{
		perror("dynamic memory allocation failure");
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < len; i++)
	{
		memcpy(&option[i], &(extd_options[i]), sizeof(struct option));
	}
	return option;
}

static void
parse_option(int argc, char** argv)
{
	int opt;
	struct option *options_table, *ptr;
	
	/* Build option table we need two pointers because getopt_long
	 * modify 'ptr' and we can't free it */
	options_table = ptr = build_option_table();
	
	while((opt = getopt_long(argc, argv, short_option_string, ptr, NULL)) != -1)
	{
		switch(opt)
		{
			case 'h':
				print_help_msg();
				break;
			case 'n':
				shm_num_items = atoi(optarg);
				break;
			case 'd':
				num_process_routine = atoi(optarg);
				break;
			case 'v':
				print_version();
				break;
		}
	}
}

int main(int argc, char** argv)
{
	// parse input options
	parse_option(argc, argv);
	
	printf("shm_num_items = %d\n", shm_num_items);
//	return 0;	
	// create daemon
	if(daemon(0, 0))
		exit(EXIT_FAILURE);
	
	// init log system
	init_log();
	// create request queue
	createRQueue();
	// create process req routines
	createProcessRoutines(); 

	while(1)
	{
		ISSD_ReqPacket_t rPacket;

		getNextReqPacket(&rPacket);
		
		dispatchReqPacket(&rPacket);
	}
	
	return 0;
}
