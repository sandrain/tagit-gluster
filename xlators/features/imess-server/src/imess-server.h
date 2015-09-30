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

#include <pthread.h>

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
};

typedef struct _ims_task ims_task_t;

struct _ims_async {
	ims_xdb_t		*xdb;
	pthread_t                worker;
	struct list_head	 task_queue;
	pthread_spinlock_t	 lock;
};

typedef struct _ims_async ims_async_t;

/*
 * private context
 */

struct _ims_priv {
	char		*db_path;
	ims_xdb_t	*xdb;

	gf_boolean_t	 lookup_cache;	/* TODO: enable lookup cache */
	gf_boolean_t	 async_update;	/* asynchronous xdb update */
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

int32_t ims_ipc_query (xlator_t *this,
		       dict_t *xdata_in, dict_t *xdata_out, int *err);

int32_t ims_ipc_filter (xlator_t *this,
		        dict_t *xdata_in, dict_t *xdata_out, int *err);

int32_t ims_ipc_extractor (xlator_t *this,
		           dict_t *xdata_in, dict_t *xdata_out, int *err);

#endif	/* _IMESS_SERVER_H_ */
