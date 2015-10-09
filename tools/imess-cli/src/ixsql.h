/* Copyright (C) 2015 	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef	_IXSQL_H_
#define	_IXSQL_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include "glfs.h"
#include "glfs-handles.h"

/* gluster internal headers */
#undef _CONFIG_H
#include "glusterfs.h"
#include "xlator.h"

#include <sys/time.h>

static inline void
timeval_latency (struct timeval *out,
		 struct timeval *before, struct timeval *after)
{
	time_t sec       = 0;
	suseconds_t usec = 0;

	if (!out || !before || !after)
		return;

	sec = after->tv_sec - before->tv_sec;
	if (after->tv_usec < before->tv_usec) {
		sec -= 1;
		usec = 1000000 + after->tv_usec - before->tv_usec;
	}
	else
		usec = after->tv_usec - before->tv_usec;

	out->tv_sec = sec;
	out->tv_usec = usec;
}

static inline double timeval_to_sec (struct timeval *t)
{
	double sec = 0.0F;

	sec += t->tv_sec;
	sec += (double) 0.000001 * t->tv_usec;

	return sec;
}

#define IXSQL_QUIT		0x12345
#define IXSQL_DEFAULT_SLICE	200

#ifndef _llu
#define _llu(x)	((unsigned long long) (x))
#endif

struct _ixsql_query {
	char           *sql;
	dict_t         *result;
	char           *operator;
	uint64_t        count;
	struct timeval  latency;
};

typedef struct _ixsql_query ixsql_query_t;

struct _ixsql_control {
	glfs_t          *gluster;
	int              direct;
	FILE            *fp_output;
	int              show_latency;
	uint64_t         slice_count;
	uint32_t         num_clients;
	int              active_clients;
	char            *volname;
	char            *volserver;
	char             cli_mask[0];		/* client mask: 1 active */
};

typedef struct _ixsql_control ixsql_control_t;

int ixsql_sql_query (ixsql_control_t *ctl, ixsql_query_t *query);

int ixsql_exec (ixsql_control_t *ctl, ixsql_query_t *query);

int ixsql_control_cmd (ixsql_control_t *control, const char *line);

#endif	/* _IXSQL_H_ */
