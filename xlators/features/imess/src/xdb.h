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

int xdb_get_count (xdb_t *xdb, char *table, uint64_t *count);

int xdb_read_all_xfile (xdb_t *xdb, dict_t *xdata);

#if 0
/**
 * xdb_insert_record: populate attributes in xdb. if the @attr is empty, this
 * will only populate the file record.
 *
 * @xdb: xdb instance
 * @file: file, will be appended if not exists already
 * @attr: array of attributes
 * @n_attr: number of elements in @attr array
 *
 * returns 0 on success, otherwise:
 * -EINVAL if the xdb is not a vaild instance
 * -EIO if database connection fails.
 */
int xdb_insert_record (xdb_t *xdb, xdb_file_t *file,
			xdb_attr_t *attr, uint64_t n_attr);

static inline int xdb_insert_file (xdb_t *xdb, xdb_file_t *file)
{
	return xdb_insert_record(xdb, file, NULL, 0);
}

int xdb_remove_file (xdb_t *xdb, xdb_file_t *file);

/**
 * xdb_insert_stat: populate the @stat attributes.
 *
 * @xdb: xdb instance
 * @file: file, will be appended if not exists already
 * @stat: stat(2) records for the @file
 *
 * returns 0 on success, otherwise:
 * -EINVAL if the xdb is not a vaild instance
 * -EIO if database connection fails.
 */
int xdb_insert_stat (xdb_t *xdb, xdb_file_t *file, struct stat *stat);

/**
 * xdb_query_files: query files with the given condition.
 *
 * @xdb: xdb instance
 * @cond: the search condition
 * @files: array of the result files, should be freed using xdb_free_files
 * @n_files: number of elements in @files array
 *
 * returns 0 on success, otherwise:
 * -EINVAL if the xdb is not a vaild instance
 */
int xdb_query_files (xdb_t *xdb, xdb_search_cond_t *cond,
			/* out */ xdb_file_t **files,
			/* out */ uint64_t n_files);

/**
 *
 *
 * @xdb
 *
 * 
 */
static inline int xdb_tx_begin (xdb_t *xdb)
{
	tx_begin(xdb);
	return 0;
}

/**
 * Commit current changes made in the index database. 
 *
 * @xdb
 *
 * 
 */
static inline int xdb_tx_commit (xdb_t *xdb)
{
	tx_commit(xdb);
	return 0;
}

int xdb_wal_checkpoint (xdb_t *xdb, int *pn_log, int *pn_chkpnt);

int xdb_measure (xdb_t *xdb, const char *query);
#endif

#endif	/* _IMESS_XDB_H_ */

