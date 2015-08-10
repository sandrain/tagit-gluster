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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sqlite3.h>

#include "xdb.h"

static const int xdb_n_tables = 3;
extern const char *xdb_schema_sqlstr;

/* pre-compiled queries */
enum {
	INSERT_FILE = 0,
	INSERT_ATTR,
	INSERT_XDATA,
	INSERT_STAT,
	GET_FID,

	XDB_N_SQLS,
};

#define __valptr(x)					\
	do {						\
		if (!(x)) return -EINVAL;		\
	} while (0);

static const char *xdb_sqls[XDB_N_SQLS] = {
/* INSERT_FILE */
	"insert or replace into xdb_file (fid, gfid, path)\n"
	"values ((select fid from xdb_file where gfid=?),?,?)\n",
/* INSERT_ATTR */
	"insert or ignore into xdb_attr (name) values (?)\n",
/* INSERT_XDATA */
	"insert or replace into xdb_xdata (fid, aid, ival, sval, val) values\n"
	"((select fid from xdb_file where gfid=?),\n"
	"(select aid from xdb_attr where name=?),?,?,?)\n",
/* INSERT_STAT */
	"insert into xdb_xdata (fid, aid, ival, sval, val)\n"
	"select ? as fid, 1 as aid, ? as ival, null as sval, ? as val\n"
	"union select ?,2,?,null,?\n"
	"union select ?,3,?,null,?\n"
	"union select ?,4,?,null,?\n"
	"union select ?,5,?,null,?\n"
	"union select ?,6,?,null,?\n"
	"union select ?,7,?,null,?\n"
	"union select ?,8,?,null,?\n"
	"union select ?,9,?,null,?\n"
	"union select ?,10,?,null,?\n"
	"union select ?,11,?,null,?\n"
	"union select ?,12,?,null,?\n"
	"union select ?,13,?,null,?\n",
/* GET_FID */
	"select fid from xdb_file where gfid=?",
};

static inline int exec_simple_sql(xdb_t *self, const char *sql)
{
	int ret = sqlite3_exec (self->conn, sql, NULL, NULL, NULL);
	return ret == SQLITE_OK ? 0 : ret;
}

static inline int tx_begin(xdb_t *self)
{
	return exec_simple_sql(self, "BEGIN TRANSACTION");
}

static inline int tx_commit(xdb_t *self)
{
	return exec_simple_sql(self, "END TRANSACTION");
}

static inline int tx_abort(xdb_t *self)
{
	return exec_simple_sql(self, "ROLLBACK");
}

/* max 1.5 GB currently */
static inline int enable_mmap(xdb_t *self)
{
	return exec_simple_sql(self, "PRAGMA mmap_size=1610612736");
}

static int get_fid(xdb_t *self, xdb_file_t *file, uint64_t *fid)
{
	int ret = 0;
	sqlite3_stmt *stmt = self->stmts[GET_FID];

	__valptr(fid);

	ret = sqlite3_bind_text(stmt, 1, file->gfid, -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret == SQLITE_ROW) {
		ret = 0;
		*fid = sqlite3_column_int(stmt, 0);
	}
	else
		ret = -ENOENT;

out:
	sqlite3_reset(stmt);
	return ret;
}

/* create tables if not exist, precompile the sqls */
static int db_initialize(xdb_t *self)
{
	int i = 0;
	int ret = 0;
	int ntables = 0;
	sqlite3_stmt *stmt = NULL;
	char *sql = "select count(type) from sqlite_master where type='table' "
		    "and name='xdb_file' or name='xdb_attr' "
		    "or name='xdb_xdata';";

	ret = sqlite3_prepare_v2(self->conn, sql, -1, &stmt, 0);
	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ntables = sqlite3_column_int(stmt, 0);
	if (ntables != xdb_n_tables) {
		ret = exec_simple_sql(self, xdb_schema_sqlstr);
		if (ret) {
			ret = -EIO;
			goto out;
		}
	}

	sqlite3_finalize(stmt);

	/* precompile sqls */
	for (i = 0; i < XDB_N_SQLS; i++) {
		ret = sqlite3_prepare_v2(self->conn, xdb_sqls[i], -1,
					 &self->stmts[i], 0);
		if (ret != SQLITE_OK) {
			ret = -EIO;
			goto out;
		}
	}

	ret = 0;	/* previously, SQLITE_ROW (=100) */
out:
	return ret;
}

static int insert_file(xdb_t *self, xdb_file_t *file)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	stmt = self->stmts[INSERT_FILE];

	ret = sqlite3_bind_text(stmt, 1, file->gfid, -1, SQLITE_STATIC);
	ret |= sqlite3_bind_text(stmt, 2, file->gfid, -1, SQLITE_STATIC);
	ret |= sqlite3_bind_text(stmt, 3, file->path, -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ret = ret == SQLITE_DONE ? 0 : -EIO;

out:
	sqlite3_reset(stmt);
	return ret;
}

static inline int
bind_stat_attr(sqlite3_stmt *stmt, uint64_t fid, int *i, uint64_t ival,
		void *val, int bytes)
{
	int ret = 0;
	int ix = *i;

	ret = sqlite3_bind_int64(stmt, ++ix, fid);
	ret |= sqlite3_bind_int64(stmt, ++ix, ival);
	ret |= sqlite3_bind_blob(stmt, ++ix, val, bytes, NULL);

	if (ret == SQLITE_OK) {
		*i = ix;
		return 0;
	}
	else
		return -EINVAL;
}

static inline int
bind_stat(sqlite3_stmt *stmt, uint64_t fid, struct stat *sb)
{
	int ret = 0;
	int i = 0;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_dev,
			     &sb->st_dev, sizeof(sb->st_dev))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_ino,
			     &sb->st_ino, sizeof(sb->st_ino))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_mode,
			     &sb->st_mode, sizeof(sb->st_mode))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_nlink,
			     &sb->st_nlink, sizeof(sb->st_nlink))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_uid,
			     &sb->st_uid, sizeof(sb->st_uid))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_gid,
			     &sb->st_gid, sizeof(sb->st_gid))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_rdev,
			     &sb->st_rdev, sizeof(sb->st_rdev))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_size,
			     &sb->st_size, sizeof(sb->st_size))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_blksize,
			     &sb->st_blksize, sizeof(sb->st_blksize))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_blocks,
			     &sb->st_blocks, sizeof(sb->st_blocks))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_atime,
			     &sb->st_atime, sizeof(sb->st_atime))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_mtime,
			     &sb->st_mtime, sizeof(sb->st_mtime))) != 0)
		goto out;

	if ((ret = bind_stat_attr(stmt, fid, &i, (uint64_t) sb->st_ctime,
			     &sb->st_ctime, sizeof(sb->st_ctime))) != 0)
		goto out;

out:
	return ret;
}

static const char *sysattrs[] = {
	"st_dev", "st_ino", "st_mode", "st_nlink", "st_uid", "st_gid",
	"st_rdev", "st_size", "st_blksize", "st_blocks",
	"st_atime", "st_mtime", "st_ctime"
};

static inline int insert_single_attr(xdb_t *self, xdb_attr_t *attr)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	stmt = self->stmts[INSERT_XDATA];

	ret = sqlite3_bind_text(stmt, 1, attr->name, -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ret = ret == SQLITE_DONE ? 0 : -EIO;

out:
	sqlite3_reset(stmt);
	return ret;
}

static int insert_attr_names(xdb_t *self, xdb_attr_t *attr, uint64_t n_attr)
{
	int ret = 0;
	uint64_t i;

	/* don't try if all attributes are sysattrs (stat) */
	for (i = 0; i < n_attr; i++)
		if (strcmp(sysattrs[i], attr[i].name))
			goto insert;

	return 0;

insert:
	for (i = 0; i < n_attr; i++) {
		ret = insert_single_attr(self, &attr[i]);
		if (ret)
			break;
	}

	return ret;
}

static int
insert_xdata(xdb_t *self, xdb_file_t *file, xdb_attr_t *attr, uint64_t n_attr)
{
	int ret = 0;
	uint64_t i;
	sqlite3_stmt *stmt = NULL;

	ret = insert_attr_names(self, attr, n_attr);
	if (ret)
		goto out;

	stmt = self->stmts[INSERT_XDATA];

	tx_begin(self);

	for (i = 0; i < n_attr; i++) {
		xdb_attr_t *current = &attr[i];

		ret = sqlite3_bind_text(stmt, 1, file->gfid,
					-1, SQLITE_STATIC);
		ret |= sqlite3_bind_text(stmt, 2, current->name,
					 -1, SQLITE_STATIC);

		if (current->type == XDB_TYPE_INTEGER)
			ret |= sqlite3_bind_int64(stmt, 3, current->ival);
		else
			ret |= sqlite3_bind_null(stmt, 3);

		if (current->type == XDB_TYPE_STRING) {
			ret |= sqlite3_bind_text(stmt, 4, current->sval,
						 -1, SQLITE_STATIC);
		}
		else
			ret |= sqlite3_bind_null(stmt, 4);

		ret |= sqlite3_bind_blob(stmt, 5, current->blob,
					 (int) current->bytes, NULL);
		if (ret) {
			tx_abort(self);
			goto out;
		}

		do {
			ret = sqlite3_step(stmt);
		} while (ret == SQLITE_BUSY);

		if (ret != SQLITE_DONE) {
			tx_abort(self);
			ret = -EIO;
			goto out;
		}

		sqlite3_reset(stmt);
	}

	tx_commit(self);
	ret = 0;

out:
	sqlite3_reset(stmt);
	return ret;
}

enum {
	XDB_ST_DEV	= 0,
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

	XDB_N_ST_ATTRS
};

static inline void
fill_attr_integer(xdb_attr_t *attr, const char *name, uint64_t ival,
		  void *mem, uint32_t size)
{
	attr->name = name;
	attr->type = XDB_TYPE_INTEGER;
	attr->ival = ival;
	attr->blob = mem;
	attr->bytes = size;
}

static inline void
fill_attr_time(xdb_attr_t *attr, const char *name, time_t *time)
{
	attr->name = name;
	attr->type = XDB_TYPE_INTEGER;
	attr->ival = (uint64_t) (*time);
	attr->blob = (void *) time;
	attr->bytes = sizeof(*time);
}

/* external interface */

int xdb_init (/* out */ xdb_t **xdb, const char *path)
{
	int ret = 0;
	sqlite3 *conn = NULL;
	xdb_t *self = NULL;

	self = calloc(1, sizeof(*self) + sizeof(sqlite3_stmt*)*XDB_N_SQLS);
	if (!self)
		return -ENOMEM;

	ret = sqlite3_open(path, &conn);
	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out_free;
	}

	self->conn = conn;
	self->dbpath = strdup(path);

	enable_mmap(self);

	ret = db_initialize(self);
	if (ret)
		goto out_free;

	*xdb = self;
	return ret;

out_free:
	free(self);
	return ret;
}

int xdb_exit (xdb_t *xdb)
{
	if (xdb) {
		if (xdb->dbpath)
			free((void *) xdb->dbpath);
		if (xdb->conn)
			sqlite3_close(xdb->conn);
		free(xdb);
	}

	return 0;
}

int xdb_insert_record (xdb_t *xdb, xdb_file_t *file,
			xdb_attr_t *attr, uint64_t n_attr)
{
	int ret = 0;

	__valptr(xdb);

	ret = insert_file(xdb, file);
	if (ret)
		return -EIO;

	if (!attr || n_attr == 0)
		return ret;

	ret = insert_xdata(xdb, file, attr, n_attr);

	return ret == 0 ? 0 : -EIO;
}

int xdb_insert_stat (xdb_t *xdb, xdb_file_t *file, struct stat *stat)
{
	int ret = 0;
	uint64_t fid = 0;
	sqlite3_stmt *stmt = NULL;

	__valptr(xdb);
	__valptr(file);
	__valptr(stat);

	stmt = xdb->stmts[INSERT_STAT];

	ret = get_fid(xdb, file, &fid);
	if (ret)
		goto out;

	ret = bind_stat(stmt, fid, stat);
	if (ret)
		goto out;

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ret = ret == SQLITE_DONE ? 0 : -EIO;
out:
	sqlite3_reset(stmt);
	return ret;
}

int xdb_query_files (xdb_t *xdb, xdb_search_cond_t *cond,
			/* out */ xdb_file_t **files,
			/* out */ uint64_t n_files)
{
	__valptr(xdb);

	return 0;
}

