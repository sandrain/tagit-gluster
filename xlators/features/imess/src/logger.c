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

#include "glusterfs.h"
#include "xlator.h"

#include "logger.h"

/* APIs */

int logger_init (logger_t **logger, const char *path,
		       int sync_mode)
{
	return 0;
}

int logger_exit (logger_t *logger)
{
	return 0;
}

int logger_append (logger_t *logger, const char *path)
{
	return 0;
}

int logger_sync (logger_t *logger)
{
	return 0;
}

