#ifndef _H_ISSD_LIB_H
#define _H_ISSD_LIB_H

extern int issd_compute_init();

extern int issd_compute(char* ifname, char* ofname, int opType);

extern int issd_compute_destroy();

#endif
