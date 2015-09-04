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

#include "xdb.h"

static const int xdb_n_tables = 3;
extern const char *xdb_schema_sqlstr;

struct xdb_callback_data {
	dict_t *xdata;
	uint64_t rows;
};
typedef struct xdb_callback_data xdb_callback_data_t;


/* pre-compiled queries */
enum {
	INSERT_FILE = 0,
	REMOVE_FILE,
	INSERT_XNAME,
	INSERT_XDATA,
	INSERT_STAT,
	REMOVE_XDATA,
	GET_FID,
	GET_COUNT_XFILE,
	GET_ALL_XFILE,
	GET_ALL_XNAME,
	GET_ALL_XDATA,

	XDB_N_SQLS,
};

#define __valptr(x)					\
	do {						\
		if (!(x)) return -EINVAL;		\
	} while (0);

static const char *xdb_sqls[XDB_N_SQLS] = {
/* INSERT_FILE (gfid, path, path, pos) */
	"insert into xdb_xfile (gfid, path, name) values (?,?,substr(?,?))\n",
/* REMOVE_FILE (gfid) */
	"delete from xdb_xfile where gfid=?\n",
/* INSERT_XNAME (name) */
	"insert or ignore into xdb_xname (name) values (?)\n",
/* INSERT_XDATA (gfid, name, ival, sval) */
	"insert or replace into xdb_xdata (fid, nid, ival, sval) values\n"
	"((select fid from xdb_xfile where gfid=?),\n"
	"(select nid from xdb_xname where name=?),?,?)\n",
/* INSERT_STAT */
	"insert into xdb_xdata (fid, nid, ival, sval)\n"
	"select ? as fid, 1 as nid, ? as ival, null as sval\n"
	"union select ?,2,?,null\n"
	"union select ?,3,?,null\n"
	"union select ?,4,?,null\n"
	"union select ?,5,?,null\n"
	"union select ?,6,?,null\n"
	"union select ?,7,?,null\n"
	"union select ?,8,?,null\n"
	"union select ?,9,?,null\n"
	"union select ?,10,?,null\n"
	"union select ?,11,?,null\n"
	"union select ?,12,?,null\n"
	"union select ?,13,?,null\n",
/* REMOVE_XDATA */
	"delete from xdb_xdata where\n"
	"fid=(select fid from xdb_xfile where gfid=?) and\n"
	"nid=(select nid from xdb_xname where name=?)\n",
/* GET_FID */
	"select fid from xdb_xfile where gfid=?",
/* GET_COUNT_XFILE */
	"select count(*) from xdb_xfile",
/* GET_ALL_XFILE */
	"select fid,gfid,path,name from xdb_xfile",
/* GET_ALL_XNAME */
	"select nid,name from xdb_xname",
/* GET_ALL_XDATA */
	"select xid,fid,nid,ival,sval from xdb_xdata",
};

/* max 1.5 GB currently */
static inline int enable_mmap(xdb_t *self)
{
	return xdb_exec_simple_sql(self, "PRAGMA mmap_size=1610612736");
}

static inline int enable_wal(xdb_t *self)
{
	/* this enables WAL and disables the automatic checkpoint which is by
	 * default performed on exceeding 1,000 pages */
	return xdb_exec_simple_sql(self,
				"PRAGMA journal_mode=WAL;"
				"PRAGMA wal_autocheckpoint=0;"
				"PRAGMA wal_checkpoint(TRUNCATE);");
}

static int get_fid(xdb_t *self, xdb_file_t *file, uint64_t *fid)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;
		
	__valptr(fid);

	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[GET_FID], -1, &stmt, 0);
	if (ret) {
		ret = -EIO;
		goto out;
	}

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
	if (stmt)
		sqlite3_finalize(stmt);

	return ret;
}

/* create tables if not exist, precompile the sqls */
static int db_initialize(xdb_t *self)
{
	int ret = 0;
	int ntables = 0;
	sqlite3_stmt *stmt = NULL;
	char *sql = "select count(type) from sqlite_master where type='table' "
		    "and name='xdb_xfile' or name='xdb_xname' "
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
		ret = xdb_exec_simple_sql(self, xdb_schema_sqlstr);
		if (ret) {
			ret = -EIO;
			goto out;
		}
	}

	ret = 0;	/* previously, SQLITE_ROW (=100) */
out:
	if (stmt)
		sqlite3_finalize(stmt);
	return ret;
}

static inline int
bind_sattr(sqlite3_stmt *stmt, uint64_t fid, int *i, uint64_t ival)
{
	int ret = 0;
	int ix = *i;

	ret = sqlite3_bind_int64(stmt, ++ix, fid);
	ret |= sqlite3_bind_int64(stmt, ++ix, ival);

	if (ret == SQLITE_OK) {
		*i = ix;
		return 0;
	}
	else
		return -EINVAL;
}

#define BIND_SATTR(s, f, i, a)						\
	do {								\
		ret = bind_sattr((s), (f), &(i), (uint64_t) (a));	\
		if (ret) goto out;					\
	} while (0);

static inline int
bind_stat(sqlite3_stmt *stmt, uint64_t fid, struct stat *sb)
{
	int ret = 0;
	int i = 0;

	BIND_SATTR(stmt, fid, i, sb->st_dev);
	BIND_SATTR(stmt, fid, i, sb->st_ino);
	BIND_SATTR(stmt, fid, i, sb->st_mode);
	BIND_SATTR(stmt, fid, i, sb->st_nlink);
	BIND_SATTR(stmt, fid, i, sb->st_uid);
	BIND_SATTR(stmt, fid, i, sb->st_gid);
	BIND_SATTR(stmt, fid, i, sb->st_rdev);
	BIND_SATTR(stmt, fid, i, sb->st_size);
	BIND_SATTR(stmt, fid, i, sb->st_blksize);
	BIND_SATTR(stmt, fid, i, sb->st_blocks);
	BIND_SATTR(stmt, fid, i, sb->st_atime);
	BIND_SATTR(stmt, fid, i, sb->st_mtime);
	BIND_SATTR(stmt, fid, i, sb->st_ctime);

out:
	return ret;
}

static const char *sysattrs[] = {
	"st_dev", "st_ino", "st_mode", "st_nlink", "st_uid", "st_gid",
	"st_rdev", "st_size", "st_blksize", "st_blocks",
	"st_atime", "st_mtime", "st_ctime"
};

static inline int insert_single_xname(xdb_t *self, xdb_attr_t *attr)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[INSERT_XNAME], -1, &stmt, 0);
	if (ret) {
		ret = -EIO;
		goto out;
	}

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
	if (ret)
		self->err = sqlite3_errmsg(self->conn);
	if (stmt)
		sqlite3_finalize(stmt);
	return ret;
}

static int insert_xnames(xdb_t *self, xdb_attr_t *attr, uint64_t n_attr)
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
		ret = insert_single_xname(self, &attr[i]);
		if (ret)
			break;
	}

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
fill_attr_integer(xdb_attr_t *attr, const char *name, uint64_t ival)
{
	attr->name = name;
	attr->type = XDB_TYPE_INTEGER;
	attr->ival = ival;
}

static inline void
fill_attr_time(xdb_attr_t *attr, const char *name, time_t *time)
{
	attr->name = name;
	attr->type = XDB_TYPE_INTEGER;
	attr->ival = (uint64_t) (*time);
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
	enable_wal(self);

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
		int pn_log = 0;
		int pn_chkpnt = 0;

		if (xdb->dbpath)
			free((void *) xdb->dbpath);
		if (xdb->conn) {
			sqlite3_wal_checkpoint_v2(xdb->conn, NULL,
					SQLITE_CHECKPOINT_TRUNCATE,
					&pn_log, &pn_chkpnt);
			sqlite3_close(xdb->conn);
		}
		free(xdb);
	}

	return 0;
}

static inline int name_substr_pos(const char *path)
{
	char *pos = rindex(path, '/');

	return (int) (((unsigned long) pos - (unsigned long) path) + 2);
}

int xdb_insert_file(xdb_t *self, xdb_file_t *file)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	__valptr(self);
	__valptr(file);

	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[INSERT_FILE], -1, &stmt, 0);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	ret = sqlite3_bind_text(stmt, 1, file->gfid, -1, SQLITE_STATIC);
	ret |= sqlite3_bind_text(stmt, 2, file->path, -1, SQLITE_STATIC);
	ret |= sqlite3_bind_text(stmt, 3, file->path, -1, SQLITE_STATIC);
	ret = sqlite3_bind_int(stmt, 4, name_substr_pos(file->path));
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ret = ret == SQLITE_DONE ? 0 : -EIO;

out:
	if (ret)
		self->err = sqlite3_errmsg(self->conn);
	if (stmt)
		sqlite3_finalize(stmt);

	return ret;
}

int xdb_remove_file (xdb_t *xdb, xdb_file_t *file)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	__valptr(xdb);
	__valptr(file);
	__valptr(file->gfid);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[REMOVE_FILE], -1,
				&stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_text(stmt, 1, file->gfid, -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE)
		ret = -EIO;
	else
		ret = 0;

out:
	if (ret)
		xdb->err = sqlite3_errmsg(xdb->conn);
	if (stmt)
		sqlite3_finalize(stmt);
	return ret;
}

int xdb_insert_stat (xdb_t *xdb, xdb_file_t *file, struct stat *stat)
{
	int ret = 0;
	uint64_t fid = 0;
	sqlite3_stmt *stmt = NULL;

	__valptr(xdb);
	__valptr(file);
	__valptr(stat);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[INSERT_STAT], -1, &stmt, 0);
	if (ret) {
		ret = -EIO;
		goto out;
	}

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
	if (ret == -EIO)
		xdb->err = sqlite3_errmsg(xdb->conn);

	sqlite3_finalize(stmt);

	return ret;
}

int xdb_insert_xattr(xdb_t *self, xdb_file_t *file, xdb_attr_t *attr,
			uint64_t n_attr)
{
	int ret = 0;
	uint64_t i;
	sqlite3_stmt *stmt = NULL;

	ret = insert_xnames(self, attr, n_attr);
	if (ret)
		goto out;

	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[INSERT_XDATA], -1, &stmt, 0);
	if (ret)
		goto out;

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

		if (ret)
			goto out;

		do {
			ret = sqlite3_step(stmt);
		} while (ret == SQLITE_BUSY);

		if (ret != SQLITE_DONE) {
			ret = -EIO;
			goto out;
		}

		sqlite3_reset(stmt);
	}

	ret = 0;

out:
	if (ret)
		self->err = sqlite3_errmsg(self->conn);
	if (stmt)
		sqlite3_finalize(stmt);

	return ret;
}

int xdb_remove_xattr (xdb_t *xdb, xdb_file_t *file, const char *name)
{
	return 0;
}

int xdb_get_count (xdb_t *xdb, char *table, uint64_t *count)
{
	int ret = 0;
	uint64_t rows = 0;
	sqlite3_stmt *stmt = NULL;

	__valptr(xdb);
	__valptr(table);
	__valptr(count);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[GET_COUNT_XFILE],
				 -1, &stmt, 0);

	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	rows = sqlite3_column_int(stmt, 0);

	/* TODO: errors */

	*count = rows;
	ret = 0;

out:
	sqlite3_finalize(stmt);
	return ret;
}

int xdb_read_all_xfile (xdb_t *xdb, dict_t *xdata)
{
	int ret = 0;
	uint64_t rows = 0;
	sqlite3_stmt *stmt = NULL;
	char keybuf[10] = {0,};
	char buf[1024] = {0,};

	__valptr(xdb);
	__valptr(xdata);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[GET_ALL_XFILE],
				 -1, &stmt, 0);

	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		sprintf(keybuf, "%llu", (unsigned long long) rows);
		sprintf(buf, "%d|%s|%s%s", sqlite3_column_int(stmt, 0),
				           sqlite3_column_text(stmt, 1),
					   sqlite3_column_text(stmt, 2),
					   sqlite3_column_text(stmt, 3));
		ret = dict_set_dynstr_with_alloc (xdata, keybuf, buf);
		if (ret) {
			ret = -EIO;
			goto out;
		}

		rows++;
	}

	if (ret != SQLITE_DONE) {
		ret = -EIO;
		goto out;
	}

	ret = dict_set_uint64 (xdata, "count", rows);
	ret = 0;

out:
	if (stmt)
		sqlite3_finalize(stmt);
	return ret;
}

int xdb_read_all_xname (xdb_t *xdb, dict_t *xdata)
{
	int ret = 0;
	uint64_t rows = 0;
	sqlite3_stmt *stmt = NULL;
	char keybuf[10] = {0,};
	char buf[1024] = {0,};

	__valptr(xdb);
	__valptr(xdata);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[GET_ALL_XNAME],
				 -1, &stmt, 0);

	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		sprintf(keybuf, "%llu", (unsigned long long) rows);
		sprintf(buf, "%d|%s", sqlite3_column_int(stmt, 0),
				      sqlite3_column_text(stmt, 1));
		ret = dict_set_dynstr_with_alloc (xdata, keybuf, buf);
		if (ret) {
			ret = -EIO;
			goto out;
		}

		rows++;
	}

	if (ret != SQLITE_DONE) {
		ret = -EIO;
		goto out;
	}

	ret = dict_set_uint64 (xdata, "count", rows);
	ret = 0;

out:
	if (stmt)
		sqlite3_finalize(stmt);
	return ret;
}

int xdb_read_all_xdata (xdb_t *xdb, dict_t *xdata)
{
	int ret = 0;
	uint64_t rows = 0;
	sqlite3_stmt *stmt = NULL;
	char keybuf[10] = {0,};
	char buf[1024] = {0,};

	__valptr(xdb);
	__valptr(xdata);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[GET_ALL_XDATA],
				 -1, &stmt, 0);

	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		sprintf(keybuf, "%llu", (unsigned long long) rows);
		sprintf(buf, "%d|%d|%d|%d|%s", sqlite3_column_int(stmt, 0),
				               sqlite3_column_int(stmt, 1),
				               sqlite3_column_int(stmt, 2),
				               sqlite3_column_int(stmt, 3),
				               sqlite3_column_text(stmt, 4));
		ret = dict_set_dynstr_with_alloc (xdata, keybuf, buf);
		if (ret) {
			ret = -EIO;
			goto out;
		}

		rows++;
	}

	if (ret != SQLITE_DONE) {
		ret = -EIO;
		goto out;
	}

	ret = dict_set_uint64 (xdata, "count", rows);
	ret = 0;

out:
	if (stmt)
		sqlite3_finalize(stmt);
	return ret;
}

static
int direct_query_callback (void *cdata, int argc, char **argv, char **colname)
{
	int ret = 0;
	int i = 0;
	xdb_callback_data_t *data = cdata;
	dict_t *xdata = data->xdata;
	uint64_t rows = data->rows;
	char keybuf[10] = {0,};
	char buf[1024] = {0,};
	char *pos = buf;

	sprintf(keybuf, "%llu", (unsigned long long) rows);

	for (i = 0; i < argc; i++) {
		pos += sprintf(pos, "%s%c", argv[i],
				i == argc - 1 ? '\0' : '|');
	}

	ret = dict_set_dynstr_with_alloc (xdata, keybuf, buf);

	if (ret)
		return ret;

	data->rows = rows + 1;
	return 0;
}

int xdb_direct_query (xdb_t *xdb, char *sql, dict_t *xdata)
{
	int ret = 0;
	xdb_callback_data_t cdata;

	__valptr(xdb);
	__valptr(sql);
	__valptr(xdata);

	cdata.rows = 0;
	cdata.xdata = xdata;

	ret = sqlite3_exec(xdb->conn, sql, &direct_query_callback, &cdata,
			   (char **) &xdb->err);
	if (ret)
		return -EIO;

	ret = dict_set_uint64 (xdata, "count", cdata.rows);

	return 0;
}

