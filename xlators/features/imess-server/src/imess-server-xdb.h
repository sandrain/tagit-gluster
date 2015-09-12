/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _IMESS_SERVER_XDB_H_
#define	_IMESS_SERVER_XDB_H_

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sqlite3.h>

enum {
	XDB_TYPE_NONE		= 0,
	XDB_TYPE_INTEGER,
	XDB_TYPE_STRING,

	N_XDB_TYPES
};

enum {
	XDB_ST_DEV		= 1,
	XDB_ST_INO,
	XDB_ST_MODE,
	XDB_ST_NLINK,
	XDB_ST_UID,
	XDB_ST_GID,
	XDB_ST_RDEV,
	XDB_ST_SIZE,
	XDB_ST_BLKSIZE,
	XDB_ST_BLOCKS,
	XDB_ST_ATIME,
	XDB_ST_MTIME,
	XDB_ST_CTIME,
};

#endif
