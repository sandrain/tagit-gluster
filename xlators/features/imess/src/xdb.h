/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _IMESS_XDB_H_
#define	_IMESS_XDB_H_

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sqlite3.h>

/*
 * our strategy is:
 * we do not maintain any consistency for the main index database, except for
 * relying on the dirty page syncing of the mmaped database file, performed by
 * the operating system. in short, for the main index database:
 *
 * pragma xdb.mmap_size = 1.5GB
 * pragma xdb.journal_mode = memory
 *
 * each record population will be a transaction which will directly modify the
 * mmaped database region. the in-memory rollback journal will be created at
 * the beginning of the transaction, and deleted at the end of the transaction.
 *
 * if system crashes, the database file would in an inconsistent status.
 * luckily, the glusterfs itself maintains the changelog which records all
 * changed records of the underlying brick. what we have to do is to read and
 * repopulate the database using this changelog feature.
 */

struct _xdb {
	sqlite3    *conn;
	const char *dbpath;
	const char *err;
};

typedef struct _xdb xdb_t;


struct _xdb_file {
	const char *gfid;
	const char *path;
};

typedef struct _xdb_file xdb_file_t;

/* TODO: ideally, XDB_TYPE_BOTH (or similar) is desired for a data to be
 * indexed both by integer and string. but this will be a future work.
 */
enum {
	XDB_TYPE_NONE		= 0,
	XDB_TYPE_INTEGER,
	XDB_TYPE_STRING,

	N_XDB_TYPES
};

enum {
	XDB_ST_DEV	= 1,
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


struct _xdb_attr {
	const char *name;	/* attribute name, for stat(2) attr, use
				   IMESS_XDB_ANAME_xx */
	int         type;	/* IMESS_XDB_TYPE_xx */
	uint64_t    ival;	/* integer parsed */
	const char *sval;	/* string parsed */
};

typedef struct _xdb_attr xdb_attr_t;

struct _xdb_search_cond {
	const char *qstr;	/* TODO: this needs to be changed */
};

typedef struct _xdb_search_cond xdb_search_cond_t;


static inline int xdb_exec_simple_sql(xdb_t *self, const char *sql)
{
	int ret = sqlite3_exec (self->conn, sql, NULL, NULL, NULL);
	return ret == SQLITE_OK ? 0 : ret;
}

static inline int xdb_tx_begin(xdb_t *self)
{
	return xdb_exec_simple_sql(self, "BEGIN TRANSACTION");
}

static inline int xdb_tx_commit(xdb_t *self)
{
	return xdb_exec_simple_sql(self, "END TRANSACTION");
}

static inline int xdb_tx_abort(xdb_t *self)
{
	return xdb_exec_simple_sql(self, "ROLLBACK");
}


/**
 * xdb_init: initialize the xdb instance. if the db file doesn't exists, this
 * will create a new database file.
 *
 * @xdb: xdb instance
 * @path: the database file path
 *
 * returns 0 on success, otherwise:
 * -EINVAL if the path is not valid
 * -ENOMEM if memory allocation fails
 * -EIO if I/O system call fails
 */
int xdb_init (/* out */ xdb_t **xdb, const char *path);

/**
 * xdb_exit: close the database connection and destroy the xdb instance.
 *
 * @xdb: xdb instance
 *
 * returns 0 on success, otherwise:
 * -EINVAL if the xdb is not a vaild instance
 * -EIO if I/O system call fails
 */
int xdb_exit (xdb_t *xdb);

/**
 * 
 *
 * @xdb
 * @file
 *
 * 
 */
int xdb_insert_file (xdb_t *xdb, xdb_file_t *file);

/**
 * 
 *
 * @xdb
 * @file
 *
 * 
 */
int xdb_remove_file (xdb_t *xdb, xdb_file_t *file);

/**
 * 
 *
 * @xdb
 * @file
 * @stat
 *
 * 
 */
int xdb_insert_stat (xdb_t *xdb, xdb_file_t *file, struct stat *stat);

/**
 * 
 *
 * @xdb
 * @file
 * @attr
 * @n_attr
 *
 * 
 */
int xdb_insert_xattr (xdb_t *xdb, xdb_file_t *file, xdb_attr_t *attr,
			uint64_t n_attr);

/**
 * 
 *
 * @xdb
 * @file
 * @name
 *
 * 
 */
int xdb_remove_xattr (xdb_t *xdb, xdb_file_t *file, const char *name);

/**
 *
 *
 * @xdb
 * @file
 * @sb
 * @att
 *
 * 
 */
int xdb_update_stat (xdb_t *xdb, xdb_file_t *file, struct stat *sb, int att);

/* the checkpoint is not necessary to be provided, since we rely on the
 * changelog xlator.
 */
static inline int xdb_set_autocheckpoint (xdb_t *xdb, int freq)
{
	int ret = 0;

	ret = sqlite3_wal_autocheckpoint(xdb->conn, freq);
	if (ret != SQLITE_OK) {
		xdb->err = sqlite3_errmsg(xdb->conn);
		return -1;
	}

	return 0;
}

#if 0
static inline
int xdb_checkpoint (xdb_t *xdb, int mode, int *pn_log, int *pn_ckpt)
{
	int ret = 0;

	if (!xdb || !xdb->conn || !pn_log || !pn_ckpt)
		return -EINVAL;

	ret = sqlite3_wal_checkpoint_v2 (xdb->conn, NULL, mode,
					pn_log, pn_ckpt);
	if (ret != SQLITE_OK)
		xdb->err = sqlite3_errmsg(xdb->conn);
	else
		ret = -EIO;

	return ret;
}


static inline int xdb_checkpoint_fast (xdb_t *xdb, int *pn_log, int *pn_ckpt)
{
	return xdb_checkpoint (xdb, SQLITE_CHECKPOINT_PASSIVE,
				pn_log, pn_ckpt);
}

static inline int xdb_checkpoint_full (xdb_t *xdb, int *pn_log, int *pn_ckpt)
{
	return xdb_checkpoint (xdb, SQLITE_CHECKPOINT_RESTART,
				pn_log, pn_ckpt);
}
#endif


/* for debug */
int xdb_read_all_xfile (xdb_t *xdb, dict_t *xdata);
int xdb_read_all_xname (xdb_t *xdb, dict_t *xdata);
int xdb_read_all_xdata (xdb_t *xdb, dict_t *xdata);

int xdb_direct_query (xdb_t *xdb, char *sql, dict_t *xdata);

#endif	/* _IMESS_XDB_H_ */

