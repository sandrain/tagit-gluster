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
#include <pthread.h>
#include <sqlite3.h>

#include "common-utils.h"
#include "dict.h"

#define _llu(x)		((unsigned long long) (x))

enum {
        IMS_XDB_TYPE_NONE       = 0,
        IMS_XDB_TYPE_INTEGER,
	IMS_XDB_TYPE_REAL,
        IMS_XDB_TYPE_STRING,
};

enum {
        IMS_XDB_STAT_OP_ALL     = 0,
        IMS_XDB_STAT_OP_WRITE,
        IMS_XDB_STAT_OP_OWNER,

        N_IMS_XDB_STAT_OPS,
};

enum {
        IMS_XDB_ST_DEV          = 1,
        IMS_XDB_ST_INO,
        IMS_XDB_ST_MODE,
        IMS_XDB_ST_NLINK,
        IMS_XDB_ST_UID,
        IMS_XDB_ST_GID,
        IMS_XDB_ST_RDEV,
        IMS_XDB_ST_SIZE,
        IMS_XDB_ST_BLKSIZE,
        IMS_XDB_ST_BLOCKS,
        IMS_XDB_ST_ATIME,
        IMS_XDB_ST_MTIME,
        IMS_XDB_ST_CTIME,

        N_IMS_XDB_ST_ATTRS      = IMS_XDB_ST_CTIME,
};

enum {
	XDB_MODE_SYNC		= 0,
	XDB_MODE_ASYNC,
	XDB_MODE_READONLY,
};

struct _ims_xdb {
        sqlite3        *conn;    /* connection to the SQLite */
        int             db_ret;  /* return value from SQLite */
	int		mode;    /* 0: SYNC, 1: ASYNC */

	dict_t         *qrset;   /* query result set for stateful processing */
};

typedef struct _ims_xdb	ims_xdb_t;

struct _ims_xdb_file {
	const char *gfid;
	const char *path;
	void       *extra;
};

typedef struct _ims_xdb_file ims_xdb_file_t;

struct _ims_xdb_attr {
	const char *gfid;       /* associated gfid */

        int         type;	/* attribute type, IMS_XDB_TYPE_xx */
        const char *name;	/* attribute name, for stat(2) use
                                   IMS_XDB_ST_xxx */
        uint64_t    ival;	/* parsed integer value */
	double      rval;	/* parsed double value */
        const char *sval;	/* string value */
};

typedef struct _ims_xdb_attr ims_xdb_attr_t;

#define __valptr(p, label)	do {                    \
        if ((p) == NULL)                                \
        goto label;                                     \
} while (0);

/*
 * ~API: not encouraged to be used directly from clients.
 */

static inline int ims_xdb_exec_simple_sql (ims_xdb_t *xdb, const char *sql)
{
        int ret   = -1;

	__valptr(xdb, out);
	__valptr(sql, out);

        ret = sqlite3_exec (xdb->conn, sql, NULL, NULL, NULL);
        if (ret == SQLITE_OK)
                return 0;
        else {
                xdb->db_ret = ret;
                return -1;
        }

out:
        return ret;
}

/*
 * API: transaction management
 */

static inline int ims_xdb_tx_begin (ims_xdb_t *xdb)
{
        return ims_xdb_exec_simple_sql (xdb, "BEGIN TRANSACTION");
}

static inline int ims_xdb_tx_commit (ims_xdb_t *xdb)
{
        return ims_xdb_exec_simple_sql (xdb, "END TRANSACTION");
}

static inline int ims_xdb_tx_rollback (ims_xdb_t *xdb)
{
        return ims_xdb_exec_simple_sql (xdb, "ROLLBACK");
}

/*
 * API: init/fini
 */

int ims_xdb_init (ims_xdb_t **xdb, const char *db_path, int mode);

int ims_xdb_exit (ims_xdb_t *xdb);

/*
 * API: file-index operations
 */

int ims_xdb_insert_gfid (ims_xdb_t *xdb, ims_xdb_file_t *file);

int ims_xdb_insert_file (ims_xdb_t *xdb, ims_xdb_file_t *file);

int ims_xdb_remove_file (ims_xdb_t *xdb, ims_xdb_file_t *file);

int ims_xdb_link_file (ims_xdb_t *xdb, ims_xdb_file_t *file);

int ims_xdb_unlink_file (ims_xdb_t *xdb, ims_xdb_file_t *file);

int ims_xdb_insert_stat (ims_xdb_t *xdb, ims_xdb_file_t *file,
                         struct stat *sb);

static inline
int ims_xdb_insert_new_file (ims_xdb_t *xdb, ims_xdb_file_t *file,
			     struct stat *sb)
{
	int ret = -1;

	__valptr (xdb, out);
	__valptr (file, out);
	__valptr (sb, out);

	ims_xdb_tx_begin (xdb);

	ret = ims_xdb_insert_gfid (xdb, file);
	if (ret)
		goto out;

	ret = ims_xdb_insert_file (xdb, file);
	if (ret)
		goto out;

	ret = ims_xdb_insert_stat (xdb, file, sb);
out:
	if (ret)
		ims_xdb_tx_rollback (xdb);
	else
		ims_xdb_tx_commit (xdb);

	return ret;
}

int ims_xdb_update_stat (ims_xdb_t *xdb, ims_xdb_file_t *file,
                         struct stat *sb, int op);

int ims_xdb_rename (ims_xdb_t *xdb, ims_xdb_file_t *file);

int ims_xdb_insert_xattr (ims_xdb_t *xdb, ims_xdb_file_t *file,
                          ims_xdb_attr_t *attr, uint64_t n_attr);

int ims_xdb_remove_xattr (ims_xdb_t *xdb, ims_xdb_file_t *file,
                          const char *name);

int ims_xdb_setxattr (ims_xdb_t *xdb, ims_xdb_attr_t *attr);

/* for now, this cannot be done asynchronously */
int ims_xdb_direct_query (ims_xdb_t *xdb, const char *sql, dict_t *xdata);

static inline const char *ims_xdb_errstr (int errcode)
{
	return sqlite3_errstr (errcode);
}

static inline
int ims_xdb_register_result (ims_xdb_t *xdb, char *key, dict_t *result)
{
	int ret = -1;

	__valptr(xdb, out);
	__valptr(result, out);
	__valptr(key, out);

	ret = dict_set_static_ptr (xdb->qrset, key, (void *) result);
out:
	return ret;
}

static inline dict_t *ims_xdb_get_result (ims_xdb_t *xdb, char *key)
{
	int ret = 0;
	void *result = NULL;

	__valptr(xdb, out);
	__valptr(key, out);

	ret = dict_get_ptr (xdb->qrset, key, &result);
	if (ret)
		result = NULL;
out:
	return (dict_t *) result;
}

#endif
