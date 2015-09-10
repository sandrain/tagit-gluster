/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef	_IMESS_LOGGER_H
#define	_IMESS_LOGGER_H

#ifndef	_CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

struct _logger {
	const char *logpath;
	int         log_fd;

	int         sync_mode;
	uint64_t    sync_count;
	pthread_t   syncer;

	pthread_spin_lock_t lock;
};

enum {
	LOGGER_SYNC_MANUAL	= 0,
	LOGGER_SYNC_PARANOID,
	LOGGER_SYNC_COUNTER,
	LOGGER_SYNC_TIMER,
};

typedef _logger logger_t;

int logger_init (logger_t **logger, const char *path,
		       int sync_mode);

int logger_exit (logger_t *logger);

int logger_append (logger_t *logger, const char *path);

int logger_sync (logger_t *logger);

#endif	/* _IMESS_LOGGER_H */

