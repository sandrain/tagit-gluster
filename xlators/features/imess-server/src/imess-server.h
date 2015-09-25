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

	gf_boolean_t	 lookup_cache;
	gf_boolean_t	 async_update;

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
 * operation code for ipc. the actual command comes as string in xdata
 */

#endif	/* _IMESS_SERVER_H_ */
