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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


#if 0
#include "glusterfs.h"
#include "xlator.h"
#endif

#include "logger.h"

static inline void logger_lock (logger_t *self)
{
	pthread_spin_lock (&self->lock);
}

static inline void logger_unlock (logger_t *self)
{
	pthread_spin_unlock (&self->lock);
}

/* APIs */

int logger_init (logger_t **logger, const char *path,
		       int sync_mode)
{
	int ret = -1;
	logger_t *self = NULL;

	self = calloc (1, sizeof(*self));
	if (!self)
		return ret;

	self->logpath = path;
	self->sync_mode = sync_mode;

	ret = pthread_spin_init (&self->lock, PTHREAD_PROCESS_PRIVATE);
	if (ret) {
		ret = -1;
		goto out_free;
	}

	ret = open (path, O_CREAT | O_APPEND | O_NOATIME | O_WRONLY, 0666);
	if (ret == -1)
		goto out_free;

	self->log_fd = ret;

	/* TODO: spawn a syncer thread for async mode */

	*logger = self;

	return 0;

out_free:
	free (self);
	return ret;
}

int logger_exit (logger_t *logger)
{
	if (logger) {
		close (logger->log_fd);
		pthread_spin_destroy (&logger->lock);
		free (logger);
	}

	return 0;
}

#if 0
int logger_append (logger_t *logger, const char *path)
{
	int ret = -1;
	int len = 0;
	char buf[2048] = {0,};

	if (!logger) {
		errno = EINVAL;
		goto out;
	}

	if (!path)
		return 0;

	len = sprintf(buf, "%s\n", path);

	logger_lock (logger);
	ret = write (logger->log_fd, buf, len);
	logger_unlock (logger);

	ret = ret == len ? 0 : -1;

out:
	return ret;
}

int logger_sync (logger_t *logger)
{
	int ret = -1;

	if (!logger) {
		errno = EINVAL;
		goto out;
	}

	if (pthread_spin_trylock(&logger->slock))
		goto out;	/* other thread is already syncing it */
	else {
		ret = fdatasync (logger->log_fd);
		pthread_spin_unlock(&logger->slock);
	}
out:
	return ret;
}
#endif

