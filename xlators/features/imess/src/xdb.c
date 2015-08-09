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
	INSERT_NEW_FILE = 0,
	INSERT_NEW_ATTR,
	INSERT_NEW_XDATA,

	N_XDB_SQLS
};

#define __valptr(x)					\
	do {						\
		if (!(x)) return -EINVAL;		\
	} while (0);

static const char *xdb_sqls[N_XDB_SQLS] = {
/* INSERT_NEW_FILE */
	"insert or replace into xdb_file (fid, gfid, path) "
	"values ((select fid from xdb_file where gfid=?),?,?)",
/* INSERT_NEW_ATTR */
	"insert or ignore into xdb_attr (name) values (?)",
/* INSERT_NEW_XDATA */
	"insert or replace into xdb_xdata (fid, aid, ival, sval, val) values "
	"((select fid from xdb_file where gfid=?),"
	"(select aid from xdb_attr where name=?),?,?,?)",
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

/* create tables if not exist */
static int db_initialize(xdb_t *self)
{
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
	ret = 0;	/* previously, SQLITE_ROW (=100) */
out:
	return ret;
}

static int insert_file(xdb_t *self, xdb_file_t *file)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;
	const char *sql = xdb_sqls[INSERT_NEW_FILE];

	ret = sqlite3_prepare_v2(self->conn, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

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
	sqlite3_finalize(stmt);
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
	const char *sql = xdb_sqls[INSERT_NEW_ATTR];

	ret = sqlite3_prepare_v2(self->conn, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

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
	sqlite3_finalize(stmt);
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
	const char *sql = xdb_sqls[INSERT_NEW_XDATA];

	ret = insert_attr_names(self, attr, n_attr);
	if (ret)
		goto out;

	ret = sqlite3_prepare_v2(self->conn, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

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
	return ret;
}

enum {
	/* let's be realistic. there exist some fields which are not searched
	 * obviously.
	 * st_dev, ino, nlink, rdev, blksize, blocks
	 */
	/*
	XDB_ST_DEV	= 0,
	XDB_ST_INO,
	XDB_ST_NLINK,
	XDB_ST_RDEV,
	XDB_ST_BLKSIZE,
	XDB_ST_BLOCKS,
	*/

	XDB_ST_MODE	= 0,
	XDB_ST_UID,
	XDB_ST_GID,
	XDB_ST_SIZE,
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

static int
convert_stat_attrs(struct stat *stat, xdb_attr_t **attr, uint64_t *n)
{
	int ret = 0;
	xdb_attr_t *attrs = NULL;

	attrs = calloc(XDB_N_ST_ATTRS, sizeof(xdb_attr_t));
	if (!attrs)
		return -ENOMEM;

#if 0
	fill_attr_integer(&attrs[XDB_ST_DEV], "st_dev", stat->st_dev,
			  &stat->st_dev, sizeof(stat->st_dev));
	fill_attr_integer(&attrs[XDB_ST_INO], "st_ino", stat->st_ino,
			  &stat->st_ino, sizeof(stat->st_ino));
	fill_attr_integer(&attrs[XDB_ST_NLINK], "st_nlink", stat->st_nlink,
			  &stat->st_nlink, sizeof(stat->st_nlink));
	fill_attr_integer(&attrs[XDB_ST_RDEV], "st_rdev", stat->st_rdev,
			  &stat->st_rdev, sizeof(stat->st_rdev));
	fill_attr_integer(&attrs[XDB_ST_BLKSIZE], "st_blksize",
			  stat->st_blksize,
			  &stat->st_blksize, sizeof(stat->st_blksize));
	fill_attr_integer(&attrs[XDB_ST_BLOCKS], "st_blocks", stat->st_blocks,
			  &stat->st_blocks, sizeof(stat->st_blocks));
#endif
	fill_attr_integer(&attrs[XDB_ST_MODE], "st_mode", stat->st_mode,
			  &stat->st_mode, sizeof(stat->st_mode));
	fill_attr_integer(&attrs[XDB_ST_UID], "st_uid", stat->st_uid,
			  &stat->st_uid, sizeof(stat->st_uid));
	fill_attr_integer(&attrs[XDB_ST_GID], "st_gid", stat->st_gid,
			  &stat->st_gid, sizeof(stat->st_gid));
	fill_attr_integer(&attrs[XDB_ST_SIZE], "st_size", stat->st_size,
			  &stat->st_size, sizeof(stat->st_size));
	fill_attr_time(&attrs[XDB_ST_ATIME], "st_atime", &stat->st_atime);
	fill_attr_time(&attrs[XDB_ST_MTIME], "st_mtime", &stat->st_mtime);
	fill_attr_time(&attrs[XDB_ST_CTIME], "st_ctime", &stat->st_ctime);

	*attr = attrs;
	*n = XDB_N_ST_ATTRS;

	return ret;
}

/* external interface */

int xdb_init (/* out */ xdb_t **xdb, const char *path)
{
	int ret = 0;
	sqlite3 *conn = NULL;
	xdb_t *self = NULL;

	self = calloc(1, sizeof(*self));
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
	xdb_attr_t *attr = NULL;
	uint64_t n;

	__valptr(xdb);
	__valptr(file);
	__valptr(stat);

	ret = convert_stat_attrs(stat, &attr, &n);
	if (ret)
		goto out;

	ret = xdb_insert_record(xdb, file, attr, n);
	free(attr);
out:
	return ret;
}

int xdb_query_files (xdb_t *xdb, xdb_search_cond_t *cond,
			/* out */ xdb_file_t **files,
			/* out */ uint64_t n_files)
{
	__valptr(xdb);

	return 0;
}

