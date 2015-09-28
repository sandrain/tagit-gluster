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
#include "ix-imess.h"

#define IXSQL_QUIT	0x12345

#ifndef _llu
#define _llu(x)	((unsigned long long) (x))
#endif

struct _ixsql_query {
	char           *sql;
	dict_t         *result;
	struct timeval  latency;
};

typedef struct _ixsql_query ixsql_query_t;

struct _ixsql_clients {
	const char *name;
	int         active;
};

typedef struct _ixsql_clients ixsql_clients_t;

struct _ixsql_control {
	glfs_t          *gluster;
	int              direct;
	FILE            *fp_output;
	int              show_latency;
	uint64_t         slice_count;
	uint32_t         num_clients;
	ixsql_clients_t  clients[0];
};

typedef struct _ixsql_control ixsql_control_t;

int ixsql_sql_query (glfs_t *fs, ixsql_query_t *query);

int ixsql_control_cmd (ixsql_control_t *control, const char *line);

#endif	/* _IXSQL_H_ */
