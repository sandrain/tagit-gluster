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

enum {
	INSERT_FILE = 0,	/* [0] */
	REMOVE_FILE,		/* [1] */
	INSERT_XNAME,		/* [2] */
	INSERT_XDATA,		/* [3] */
	REMOVE_XDATA,		/* [4] */
	INSERT_STAT,		/* [5] */
	UPDATE_STAT,		/* [6] */
	GET_FID,		/* [7] */
	GET_ALL_XFILE,		/* [8] */
	GET_ALL_XNAME,		/* [9] */
	GET_ALL_XDATA,		/* [10] */

	XDB_N_SQLS,
};

static const char *xdb_sqls[XDB_N_SQLS] = {
/* [0] INSERT_FILE (gfid, path, path, pos) */
	"insert into xdb_xfile (gfid, path, name) values (?,?,substr(?,?))\n",
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
/* [5] INSERT_STAT */
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
/* [6] UPDATE_STAT (ival, gfid, nid) */
	"update xdb_xdata set ival=?\n"
	"where fid=(select fid from xdb_xfile where gfid=?) and nid=?\n",
/* [7] GET_FID (gfid) */
	"select fid from xdb_xfile where gfid=?",
/* [8] GET_ALL_XFILE */
	"select fid,gfid,path,name from xdb_xfile",
/* [9] GET_ALL_XNAME */
	"select nid,name from xdb_xname",
/* [10] GET_ALL_XDATA */
	"select xid,fid,nid,ival,sval from xdb_xdata",
};

