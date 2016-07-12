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

/*
 * helpers
 */

static inline ims_task_t *task_dup (ims_task_t *task)
{
	ims_task_t *cp_task  = NULL;
	ims_xdb_file_t *file = NULL;
	ims_xdb_attr_t *attr = NULL;

	cp_task = GF_CALLOC (1, sizeof(*cp_task), gf_ims_mt_ims_task_t);
	if (cp_task) {
		*cp_task = *task;

		/* the path is lost after fs calls are finished. so, allocate a
		 * new one for each.
		 */
		file = &cp_task->file;

		if (task->file.gfid)
			file->gfid = gf_strdup (task->file.gfid);
		if (task->file.path)
			file->path = gf_strdup (task->file.path);
		if (task->file.extra)
			file->extra = gf_strdup ((char *) task->file.extra);

		attr = &cp_task->attr;

		if (task->attr.gfid)
			attr->gfid = gf_strdup (task->attr.gfid);
		if (task->attr.name)
			attr->name = gf_strdup (task->attr.name);
		if (task->attr.sval)
			attr->sval = gf_strdup (task->attr.sval);
	}

	return cp_task;
}

static inline void task_destroy (ims_task_t *task)
{
	if (task) {
		if (task->file.gfid)
			GF_FREE ((char *) task->file.gfid);
		if (task->file.extra)
			GF_FREE (task->file.extra);
		if (task->file.path)
			GF_FREE ((char *) task->file.path);
		if (task->attr.gfid)
			GF_FREE ((char *) task->attr.gfid);
		if (task->attr.name)
			GF_FREE ((char *) task->attr.name);
		if (task->attr.sval)
			GF_FREE ((char *) task->attr.sval);

		GF_FREE (task);
	}
}

/*
 * task queue management
 */

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
	return ims_xdb_setxattr (self->xdb, &task->attr);
}

static inline int worker_remove_xattr (ims_async_t *self, ims_task_t *task)
{
	return 0;
}

static const char *opstrs[] = {
	"create_file", "unlink_file", "update_stat", "link_file",
	"rename", "insert_xattr", "remove_xattr"
};

static void *ims_async_work (void *arg)
{
	int ret			= 0;
	uint64_t sleep_time	= 0;
	ims_async_t *self	= NULL;
	ims_xdb_t *xdb 		= NULL;
	ims_task_t *task	= NULL;
	double timegap          = 0.0;

	self = arg;
	xdb = self->xdb;

	while (1) {
		task = task_queue_fetch (self);
		if (!task) {
			sleep_time = 1000;	/* 1 miscrosec */
			usleep (sleep_time);
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
			gf_log ("ims_async", GF_LOG_WARNING,
				"ims_async_work: unknown task opcode: %d",
				task->op);
			break;
		}

		if (ret)
			gf_log ("ims_async", GF_LOG_WARNING,
				"ims_async_work: xdb operation failed "
				"(ret=%d, db_ret=%d, op=%d, file=%s, gfid=%s)",
				ret, xdb->db_ret,
				task->op, task->file.path, task->file.gfid);

		if (self->latency_log) {
			gettimeofday (&task->t_complete, NULL);

			timegap = timegap_double (&task->t_enqueue,
					          &task->t_complete);

			gf_log ("ims_async", GF_LOG_INFO,
				"(%s) queue_time = %.6lf sec",
				opstrs[task->op], timegap);
		}

		task_destroy (task);
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
	int ret 	     = -1;
	ims_task_t *cp_task  = NULL;

	cp_task = task_dup (task);
	if (!cp_task)
		goto out;

	if (self->latency_log)
		gettimeofday (&cp_task->t_enqueue, NULL);
	task_queue_append (self, cp_task);

	ret = 0;
out:
	return ret;
}

