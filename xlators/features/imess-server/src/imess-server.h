/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _IMESS_SERVER_H_
#define	_IMESS_SERVER_H_

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

#include "glusterfs.h"
#include "xlator.h"
#include "common-utils.h"
#include "logging.h"
#include "options.h"
#include "dict.h"

#include "imess-server-mem-types.h"
#include "imess-server-xdb.h"

/*
 * asynchronous update
 */

enum {
	IMS_TASK_INSERT_NEW_FILE	= 0,
	IMS_TASK_INSERT_STAT,
	IMS_TASK_UPDATE_STAT,
	IMS_TASK_LINK_FILE,
	IMS_TASK_UNLINK_FILE,
	IMS_TASK_RENAME,
	IMS_TASK_INSERT_XATTR,
	IMS_TASK_REMOVE_XATTR,

	N_IMS_TASK_OP,
};

struct _ims_task {
	struct list_head  list;

	int		  op;
	ims_xdb_file_t	  file;
	struct stat	  sb;

	ims_xdb_attr_t	  attr;

	/* timestamp to record queue delay */
	struct timeval    t_enqueue;
	struct timeval    t_complete;
};

typedef struct _ims_task ims_task_t;

struct _ims_async {
	ims_xdb_t		*xdb;
	pthread_t                worker;
	struct list_head	 task_queue;
	pthread_spinlock_t	 lock;

	gf_boolean_t             async_log;
};

typedef struct _ims_async ims_async_t;

/*
 * private context
 */

struct _ims_priv {
	char            *brick_path;	/* FIXME: this should be acquired from
					   the storage/posix
					   NOTE: no slash at the end */
	char		*db_path;
	ims_xdb_t	*xdb;

	gf_boolean_t	 lookup_cache;	/* TODO: enable lookup cache */
	gf_boolean_t	 async_update;	/* asynchronous xdb update */
	gf_boolean_t     async_log;	/* asynchronous log switch */
	gf_boolean_t	 dir_hash;	/* store dir index entry only in a
					   hashed location */

	ims_async_t	*async_ctx;
};

typedef struct _ims_priv ims_priv_t;

/*
 * ims-server-async.c
 */

int ims_async_init (ims_async_t **ctx, ims_xdb_t *xdb);

void ims_async_exit (ims_async_t *ctx);

/*
 * use stack variable for @task, instead of malloc. the task is internally
 * copied to a dynamically allocated space.
 */
int ims_async_put_task (ims_async_t *ctx, ims_task_t *task);

/*
 * imess ipc operations (imess-server-active.c)
 */

/*
 * we support two ipc requests:
 *
 * 1. sql query to fetch entries from xdb
 * 2. active execution request (filter, extractor)
 *
 * ## 1. sql queries
 * - request format:
 *   @op: IMESS_IPC_OP
 *   @xdata: [ 'type': 'query', 'sql': '<any sql queries>' ]
 *
 * - the query result data is encoded into a new dict (xdata) as follows:
 *   @xdata(out): [ 'count': <number of records>,
 *                  0: <first row>, 1: <second row>, ... ]
 *
 * - all concerns about the query (e.g. distructive sqls) should be taken care
 *   of in the client application. here, we do nothing but executing whatever
 *   queries received.
 *
 * ## 2. active requests
 * - follow the concept of 'find .. -exec ..'
 * - server will run any specified code with each file (from the query result),
 *   and the result will be back to the application.
 *
 * - request format:
 *   @op: IMESS_IPC_OP
 *   @xdata: [ 'type': 'exec', 'sql': any sql returns file list,
 *             'operator': 'path' ]
 *
 * - return format:
 *   @xdata(out): [ 'from': <the xlator returns this result>,
 *                  'ret': <return code from the execution (int32_t)>,
 *                  'runtime': <time taken in seconds (double)>,
 *                  'count': <number of output lines (uint64_t)>,
 *                  '0': <first line>, '1': <second line>, (char *) ... ]
 *
 * --- below obsolte (probably future work) ---
 * ## 2. active requests
 * - currently two types of active operations are supported, a filter and an
 *   extractor.
 *
 * - request format:
 *   @op: IMESS_IPC_OP
 *   @xdata: [ 'type': 'filter' or 'extractor',
 *             'sql': '<sql for files to be affected>',
 *             'operator': '<program pathname for filter or extractor>' ]
 *
 * - the filter is more a transformer, which allows applications to read
 *   processed file data, instead of the raw data. this aims for reducing
 *   unnecessary file system traffic only to do minor tasks, such as image
 *   conversion, and data extraction (nc files). the filter program should
 *   take two arguments, one for input data file and the other for the output
 *   data file. the program should work like:
 *
 *   (shell) $ filter input.txt input.txt-filtered-@34
 *   (shell) $ echo $?
 *   0
 *   (shell) $
 *
 *   the output file is not included in the file system, but can be only read
 *   through the .meta entry. your filter is run on-the-fly upon an application
 *   is reading it via .meta interface.
 *
 * - the extractor allows applications to extract any attributes from a file,
 *   to be index by our xdb. the extractor should take an argument as a
 *   filename and emit output lines, each of which strictly formatted as
 *
 *   '<key>'='<type prefix>''<value>'
 *
 *   . two type prefixes are recognized: 'i' for numeric values, and 's' for
 *   string values. floating point is not supported, yet. for example, if the
 *   extractor, setavg reads a file, 'result.txt' and wants to set 'average'=30
 *   and 'title'='job31', the program should work as:
 *
 *   (shell) $ setavg /any/where/result.txt
 *   average=i30
 *   title=sjob31
 *   (shell) $ 
 *
 *   the result.txt will be set with additional extended attributes: average=30
 *   and title=job31. if the file is already set with an attribute name, the
 *   corresponding value will be overwritten.
 */

#define IMS_IPC_TYPE_UNKNOWN	0
#define	IMS_IPC_TYPE_EXEC	1
#define	IMS_IPC_TYPE_FILTER	2
#define	IMS_IPC_TYPE_EXTRACTOR	3

int32_t ims_ipc_query (xlator_t *this,
		       dict_t *xdata_in, dict_t *xdata_out, int *err);

int32_t
ims_ipc_active_exec (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out,
			int *err, int type);

static inline int32_t
ims_ipc_exec (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	return ims_ipc_active_exec (this, xdata_in, xdata_out, err,
					IMS_IPC_TYPE_EXEC);
}

static inline int32_t
ims_ipc_filter (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	return ims_ipc_active_exec (this, xdata_in, xdata_out, err,
					IMS_IPC_TYPE_FILTER);
}

static inline int32_t
ims_ipc_extractor (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out,
			int *err)
{
	return ims_ipc_active_exec (this, xdata_in, xdata_out, err,
					IMS_IPC_TYPE_EXTRACTOR);
}

/*
 * helpers
 */

/*
 * xattr helpers
 */

struct _ims_xattr_filler {
	int             count;
	ims_xdb_attr_t *xattr;
};

typedef struct _ims_xattr_filler ims_xattr_filler_t;

static inline int
ims_find_setxattr_kv (dict_t *dict, char *k, data_t *v, void *tmp)
{
	int ret                      = 0;
	int type                     = 0;
	ims_xattr_filler_t *filler   = NULL;
	char data_str[PATH_MAX]      = { 0, };
	int64_t ival	             = 0;
	double rval                  = .0f;

	filler = tmp;

	if (!XATTR_IS_IMESSXDB (k))
		goto out;

	memset (data_str, 0, sizeof(data_str));
	memcpy (data_str, data_to_ptr (v), v->len);

	if (strchr (data_str, '.'))
		goto try_real;

	ret = sscanf (data_str, "%ld", &ival);
	if (ret == 1) {
		type = IMS_XDB_TYPE_INTEGER;
		goto done;
	}

try_real:
	ret = sscanf (data_str, "%lf", &rval);
	if (ret == 1)
		type = IMS_XDB_TYPE_REAL;
	else
		type = IMS_XDB_TYPE_STRING;

done:
	filler->xattr->name = k;
	filler->xattr->type = type;
	filler->count++;

	filler->xattr->ival = 0;
	filler->xattr->rval = .0f;
	filler->xattr->sval = NULL;

	switch (type) {
	case IMS_XDB_TYPE_INTEGER:
		filler->xattr->ival = ival;
		break;
	case IMS_XDB_TYPE_REAL:
		filler->xattr->rval = rval;
		break;
	case IMS_XDB_TYPE_STRING:
		filler->xattr->sval = gf_strdup (data_str);
		break;
	default:
		break;
	}

out:
	return 0;
}

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


static inline double
timegap_double (struct timeval *before, struct timeval *after)
{
	struct timeval lat = { 0, };

	timeval_latency (&lat, before, after);

	return timeval_to_sec (&lat);
}


#endif	/* _IMESS_SERVER_H_ */
