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

#include "imess-server-xdb.h"

/*
 * SQL statements
 */

extern const char *ims_xdb_schema_sqlstr;
static const int xdb_n_tables = 3;

enum {
	TABLE_EXISTS = 0,	/* [0] */
	PRAGMA,			/* [1] */
	GET_FID,		/* [2] */
	INSERT_FILE,		/* [3] */
	INSERT_STAT,		/* [4] */

	REMOVE_FILE,		/* [2] */
	INSERT_XNAME,		/* [3] */
	INSERT_XDATA,		/* [4] */
	REMOVE_XDATA,		/* [5] */
	UPDATE_STAT,		/* [7] */
	GET_ALL_XFILE,		/* [9] */
	GET_ALL_XNAME,		/* [10] */
	GET_ALL_XDATA,		/* [11] */

	XDB_N_SQLS,
};

static const char *xdb_sqls[XDB_N_SQLS] = {
/* [0] TABLE_EXISTS */
	"select count(type) from sqlite_master where type='table'\n"
	"and (name='xdb_xfile' or name='xdb_xname' or name='xdb_xdata')\n",
/* [1] PRAGMA */
	"pragma mmap_size=1610612736;\n"
	"pragma journal_mode=memory;\n"
	"pragma synchronous=0;\n"
	"pragma temp_store=2;\n",
/* [2] GET_FID (gfid) */
	"select fid from xdb_xfile where gfid=?",
/* [3] INSERT_FILE (gfid, path, path, pos) */
	"insert into xdb_xfile (gfid, path, name) values (?,?,substr(?,?))\n",
/* [4] INSERT_STAT */
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

/* [1] REMOVE_FILE (gfid) */
	"delete from xdb_xfile where gfid=?\n",
/* [2] INSERT_XNAME (name) */
	"insert or ignore into xdb_xname (name) values (?)\n",
/* [3] INSERT_XDATA (gfid, name, ival, sval) */
	"insert or replace into xdb_xdata (fid, nid, ival, sval) values\n"
	"((select fid from xdb_xfile where gfid=?),\n"
	"(select nid from xdb_xname where name=?),?,?)\n",
/* [4] REMOVE_XDATA (gfid, name) */
	"delete from xdb_xdata where\n"
	"fid=(select fid from xdb_xfile where gfid=?) and\n"
	"nid=(select nid from xdb_xname where name=?)\n",
/* [6] UPDATE_STAT (ival, gfid, nid) */
	"update xdb_xdata set ival=?\n"
	"where fid=(select fid from xdb_xfile where gfid=?) and nid=?\n",
/* [8] GET_ALL_XFILE */
	"select fid,gfid,path,name from xdb_xfile",
/* [9] GET_ALL_XNAME */
	"select nid,name from xdb_xname",
/* [10] GET_ALL_XDATA */
	"select xid,fid,nid,ival,sval from xdb_xdata",
};

/*
 * direct query callback
 */
struct _ims_xdb_callback_data {
	dict_t	*xdata;
	uint64_t rows;
};

typedef struct _ims_xdb_callback_data ims_xdb_callback_data_t;

/* helper functions */

static int db_initialize (ims_xdb_t *self)
{
	int ret		   = -1;
        int db_ret         = 0;
	int n_tables	   = 0;
	sqlite3_stmt *stmt = NULL;

	db_ret = sqlite3_prepare_v2 (self->conn, xdb_sqls[TABLE_EXISTS], -1,
			          &stmt, 0);
	if (db_ret != SQLITE_OK)
		goto out;

	db_ret = sqlite3_step (stmt);
	if (db_ret != SQLITE_ROW)
		goto out;

	n_tables = sqlite3_column_int (stmt, 0);

	if (n_tables != xdb_n_tables) {
		db_ret = ims_xdb_exec_simple_sql (self, ims_xdb_schema_sqlstr);
		if (db_ret)
			goto out;
	}

	db_ret = ims_xdb_exec_simple_sql (self, xdb_sqls[PRAGMA]);
	if (db_ret)
		goto out;

	ret = 0;
out:
        self->db_ret = db_ret;
	if (stmt)
		sqlite3_finalize (stmt);
	return ret;
}

static int path_filename (const char *path)
{
	char *pos = NULL;

	pos = rindex (path, '/');
	return (int) (((unsigned long) pos - (unsigned long) path) + 2);
}

static int get_fid (ims_xdb_t *self, ims_xdb_file_t *file, uint64_t *fid)
{
        int ret            = -1;
        int db_ret         = 0;
        sqlite3_stmt *stmt = NULL;

        db_ret = sqlite3_prepare_v2 (self->conn, xdb_sqls[GET_FID],
                                     -1, &stmt, 0);
        if (db_ret)
                goto out;

        db_ret = sqlite3_bind_text (stmt, 1, file->gfid, -1, SQLITE_STATIC);
        if (db_ret)
                goto out;

        do {
                db_ret = sqlite3_step (stmt);
        } while (db_ret == SQLITE_BUSY);

        if (db_ret == SQLITE_ROW) {
                *fid = sqlite3_column_int (stmt, 0);
                ret = 0;
        }
out:
        self->db_ret = db_ret;
        if (stmt)
                sqlite3_finalize (stmt);
        return ret;
}

static inline int
bind_sattr(sqlite3_stmt *stmt, uint64_t fid, int *i, uint64_t ival)
{
        int ret = 0;
        int ix  = 0;

        ix = *i;

        ret = sqlite3_bind_int64(stmt, ++ix, fid);
        ret |= sqlite3_bind_int64(stmt, ++ix, ival);

        if (ret == SQLITE_OK) {
                *i = ix;
                return 0;
        }
        else
                return -EINVAL;
}

#define BIND_SATTR(s, f, i, a)                                          \
        do {                                                            \
                ret = bind_sattr((s), (f), &(i), (uint64_t) (a));       \
                if (ret) goto out;                                      \
        } while (0);

static inline int
bind_stat(sqlite3_stmt *stmt, uint64_t fid, struct stat *sb)
{
        int ret = 0;
        int i   = 0;

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

static
int direct_query_callback (void *cdata, int argc, char **argv, char **colname)
{
        int ret                       = 0;
        int i                         = 0;
        dict_t *xdata                 = NULL;
        uint64_t rows                 = 0;
        ims_xdb_callback_data_t *data = NULL;
        char keybuf[10]               = {0,};
        char buf[1024]                = {0,};
        char *pos                     = NULL;

        data = cdata;
        xdata = data->xdata;
        rows = data->rows;
        pos = buf;

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

/*
 * API: init/fini
 */

int ims_xdb_init (ims_xdb_t **self, const char *db_path)
{
	int ret         = -1;
	sqlite3 *conn   = NULL;
	ims_xdb_t *_self = NULL;

	__valptr (db_path, out);

	_self = CALLOC (1, sizeof(*_self));
	if (!_self)
		goto out;

	ret = sqlite3_open (db_path, &conn);
	if (ret != SQLITE_OK) {
		ret = -1;
		errno = EIO;
		goto out;
	}

	_self->conn = conn;

	ret = db_initialize (_self);
	if (ret)
		goto out;

	*self = _self;

	return 0;

out:
	if (_self)
		FREE (_self);
	return ret;
}

int ims_xdb_exit (ims_xdb_t *self)
{
	if (self) {
		if (self->conn)
			sqlite3_close (self->conn);
		FREE (self);
	}

	return 0;
}

/*
 * API: file-index operations
 */

int ims_xdb_insert_file (ims_xdb_t *self, ims_xdb_file_t *file)
{
        int ret		   = -1;
        int db_ret         = 0;
        sqlite3_stmt *stmt = NULL;

        __valptr(self, out);
        __valptr(file, out);

        db_ret = sqlite3_prepare_v2 (self->conn, xdb_sqls[INSERT_FILE],
                                     -1, &stmt, 0);
        if (db_ret)
                goto out;

        db_ret = sqlite3_bind_text (stmt, 1, file->gfid, -1, SQLITE_STATIC);
        db_ret |= sqlite3_bind_text (stmt, 2, file->path, -1, SQLITE_STATIC);
        db_ret |= sqlite3_bind_text (stmt, 3, file->path, -1, SQLITE_STATIC);
        db_ret = sqlite3_bind_int (stmt, 4, path_filename(file->path));
        if (db_ret)
                goto out;

        do {
                db_ret = sqlite3_step (stmt);
        } while (db_ret == SQLITE_BUSY);

        ret = db_ret == SQLITE_DONE ? 0 : -1;
out:
        self->db_ret = db_ret;
        if (stmt)
                sqlite3_finalize (stmt);
        return ret;
}

int ims_xdb_remove_file (ims_xdb_t *self, ims_xdb_file_t *file)
{
	return 0;
}

int ims_xdb_insert_stat (ims_xdb_t *self, ims_xdb_file_t *file,
                struct stat *sb)
{
        int ret            = -1;
        int db_ret         = 0;
        uint64_t fid       = 0;
        sqlite3_stmt *stmt = NULL;

        __valptr (self, out);
        __valptr (file, out);
        __valptr (sb, out);

        db_ret = sqlite3_prepare_v2 (self->conn, xdb_sqls[INSERT_STAT],
                                     -1, &stmt, 0);
        if (db_ret)
                goto out;

        ret = get_fid (self, file, &fid);
        if (ret)
                goto out;

        ret = bind_stat (stmt, fid, sb);
        if (ret)
                goto out;

        do {
                db_ret = sqlite3_step (stmt);
        } while (db_ret == SQLITE_BUSY);

        ret = db_ret == SQLITE_DONE ? 0 : -1;
out:
        self->db_ret = db_ret;
        if (stmt)
                sqlite3_finalize (stmt);

        return ret;
}

int ims_xdb_update_stat (ims_xdb_t *self, ims_xdb_file_t *file,
                         struct stat *sb, int attr)
{
	return 0;
}

int ims_xdb_insert_xattr (ims_xdb_t *self, ims_xdb_file_t *file,
                          ims_xdb_attr_t *attr, uint64_t n_attr)
{
	return 0;
}

int ims_xdb_remove_xattr (ims_xdb_t *self, ims_xdb_file_t *file,
                          const char *name)
{
	return 0;
}

int ims_xdb_direct_query (ims_xdb_t *self, const char *sql, dict_t *xdata)
{
        int ret                       = -1;
        int db_ret		      = 0;
        ims_xdb_callback_data_t cdata = { 0, };

        __valptr (self, out);
        __valptr (sql, out);
        __valptr (xdata, out);

        cdata.rows = 0;
        cdata.xdata = xdata;

        db_ret = sqlite3_exec (self->conn, sql, &direct_query_callback,
                        &cdata, NULL);
        if (db_ret)
                goto out;

        ret = dict_set_uint64 (xdata, "count", cdata.rows);

out:
        self->db_ret = db_ret;
        return ret;
}

