/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "glusterfs.h"
#include "glusterfs-acl.h"
#include "xlator.h"
#include "logging.h"
#include "list.h"

#include "imess-server.h"

static inline int lock_task_queue (ims_async_t *self)
{
	return pthread_spin_lock (&self->lock);
}

static inline void unlock_task_queue (ims_async_t *self)
{
	pthread_spin_unlock (&self->lock);
}

static inline void task_queue_append (ims_async_t *self, ims_task_t *task)
{
	lock_task_queue (self);
	{
		list_add_tail (&task->list, &self->task_queue);
	}
	unlock_task_queue (self);
}

static inline ims_task_t *task_queue_fetch (ims_async_t *self)
{
	ims_task_t *task = NULL;

	lock_task_queue (self);
	{
		if (list_empty (&self->task_queue))
			goto unlock;

		task = list_first_entry (&self->task_queue, ims_task_t, list);
		list_del (&task->list);
	}
unlock:
	unlock_task_queue (self);

	return task;
}

/*
 * thread working function
 *
 * any errors, we have no hope.
 */

static inline int worker_insert_new_file (ims_async_t *self, ims_task_t *task)
{
	return ims_xdb_insert_new_file (self->xdb, &task->file, &task->sb);
}

static inline int worker_unlink_file (ims_async_t *self, ims_task_t *task)
{
	return ims_xdb_unlink_file (self->xdb, &task->file);
}

static inline int worker_update_stat (ims_async_t *self, ims_task_t *task)
{
	return ims_xdb_update_stat (self->xdb, &task->file, &task->sb,
				    IMS_XDB_STAT_OP_ALL);
}

static inline int worker_link_file (ims_async_t *self, ims_task_t *task)
{
	return ims_xdb_link_file (self->xdb, &task->file);
}

static inline int worker_rename (ims_async_t *self, ims_task_t *task)
{
	return ims_xdb_rename (self->xdb, &task->file);
}

static inline int worker_insert_xattr (ims_async_t *self, ims_task_t *task)
{
	return 0;
}

static inline int worker_remove_xattr (ims_async_t *self, ims_task_t *task)
{
	return 0;
}

static void *ims_async_work (void *arg)
{
	int ret			= 0;
	ims_async_t *self	= NULL;
	ims_xdb_t *xdb 		= NULL;
	ims_task_t *task	= NULL;

	self = arg;
	xdb = self->xdb;

	while (1) {
		task = task_queue_fetch (self);
		if (!task) {
			usleep (5000);
			continue;
		}

		switch (task->op) {
		case IMS_TASK_INSERT_NEW_FILE:
			ret = worker_insert_new_file (self, task);
			break;
		case IMS_TASK_UNLINK_FILE:
			ret = worker_unlink_file (self, task);
			break;
		case IMS_TASK_UPDATE_STAT:
			ret = worker_update_stat (self, task);
			break;
		case IMS_TASK_LINK_FILE:
			ret = worker_link_file (self, task);
			break;
		case IMS_TASK_RENAME:
			ret = worker_rename (self, task);
			break;
		case IMS_TASK_INSERT_XATTR:
			ret = worker_insert_xattr (self, task);
			break;
		case IMS_TASK_REMOVE_XATTR:
			ret = worker_remove_xattr (self, task);
			break;
		default:
			/* how to report this? */
			break;
		}

		GF_FREE (task);
	}

	return (void *) 0;
}

/*
 * API
 */

int ims_async_init (ims_async_t **self, ims_xdb_t *xdb)
{
	int ret = -1;
	ims_async_t *_self = NULL;

	__valptr (xdb, out);

	_self = GF_CALLOC (1, sizeof(*_self), gf_ims_mt_ims_async_t);
	if (!self)
		goto out;

	INIT_LIST_HEAD (&_self->task_queue);
	pthread_spin_init (&_self->lock, PTHREAD_PROCESS_PRIVATE);
	_self->xdb = xdb;

	ret = pthread_create (&_self->worker, NULL, ims_async_work, _self);
	if (ret)
		goto out;

	*self = _self;
	ret = 0;
out:
	if (ret && _self) {
		pthread_spin_destroy (&_self->lock);
		GF_FREE (_self);
	}

	return ret;
}

void ims_async_exit (ims_async_t *self)
{
	if (self) {
		if (self->xdb)
			ims_xdb_exit (self->xdb);

		pthread_spin_destroy (&self->lock);

		GF_FREE (self);
	}
}

int ims_async_put_task (ims_async_t *self, ims_task_t *task)
{
	int ret = -1;
	ims_task_t *cp_task = NULL;

	cp_task = GF_CALLOC (1, sizeof(*cp_task), gf_ims_mt_ims_task_t);
	if (!cp_task)
		goto out;

	*cp_task = *task;
	task_queue_append (self, cp_task);

	ret = 0;
out:
	return ret;
}

