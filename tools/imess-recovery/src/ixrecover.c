/* Copyright (C) 2016 	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * NOTE: This program is only for performance demonstratin purposes.
 */

#ifndef _CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <unistd.h>
#include <sqlite3.h>
#include <attr/xattr.h>

#include "ixrecover.h"

/* FIXME: not sure what exactly this limit is */
#define MAX_N_XATTRS	128
#define MAX_XATTR_NAME	256
#define	MAX_XATTR_VAL	65536
#define GFID_LEN	32

#define	_llu(x)		((unsigned long long) (x))

struct xattr {
	char *name;
	char val[MAX_XATTR_VAL];
	uint32_t len;
};

struct file_attributes {
	const char *file;
	char gfid[GFID_LEN];
	struct stat stat;
	struct xattr xattrs[MAX_N_XATTRS];
	uint32_t n_xattrs;

	/* database row ids */
	uint64_t gid;
	uint64_t fid;
};

/* a single sharaed buffer for the entire process.
 * we process one file at a time for now, without any optimizations, e.g.,
 * parallel execution, producer/consumer threads, ...
 */
static struct file_attributes _fattr;
static struct file_attributes *fattr = &_fattr;
static char xattrlist[MAX_N_XATTRS*MAX_XATTR_NAME];

static uint64_t gfid_num;

/* runtime options */
static int verbose;
static uint64_t count;
#define vmsg(...)	do { if (verbose) printf("# " __VA_ARGS__); } while (0)

/* program arguments */
static char *logfile;
static char *dbpath;
static sqlite3 *conn;
static char linebuf[512];

/* runtime statistics */
static uint64_t total_files;
static uint64_t total_xattrs;
static struct timeval t_recovery_begin;
static struct timeval t_recovery_end;

/*
 * timing calculation
 */
static inline void
timeval_latency(struct timeval *out,
		struct timeval *before, struct timeval *after)
{
	time_t sec       = 0;
	suseconds_t usec = 0;

	if (!out || !before || !after)
		return;

	sec = after->tv_sec - before->tv_sec;
	if (after->tv_usec < before->tv_usec) {
		sec -= 1;
		usec = 1000000 + after->tv_usec - before->tv_usec;
	}
	else
		usec = after->tv_usec - before->tv_usec;

	out->tv_sec = sec;
	out->tv_usec = usec;
}

static inline double timeval_to_sec(struct timeval *t)
{
	double sec = 0.0F;

	sec += t->tv_sec;
	sec += (double) 0.000001 * t->tv_usec;

	return sec;
}

static inline double
timegap_double(struct timeval *before, struct timeval *after)
{
	struct timeval lat = { 0, };

	timeval_latency(&lat, before, after);

	return timeval_to_sec(&lat);
}

/*
 * database related
 */

enum {
	/* [0] */ INSERT_GFID = 0,
	/* [1] */ INSERT_FILE,
	/* [2] */ INSERT_STAT,
	/* [3] */ INSERT_XNAME,
	/* [4] */ INSERT_XDATA,

	N_SQLS,
};

static const char *sqls[N_SQLS] = {
	/* [0] INSERT_GFID (gfid) */
	"insert into xdb_xgfid (gfid) values (?)\n",
	/* [1] INSERT_FILE */
	"insert into xdb_xfile (gid, path, name) values \n"
		"(?,?,substr(?,?))\n",
	/* [2] INSERT_STAT */
	"insert into xdb_xdata (gid, nid, ival, rval, sval)\n"
		"select ? as gid, 1 as nid, ? as ival,\n"
		"null as rval, null as sval\n"
		"union select ?,2,?,null,null\n"
		"union select ?,3,?,null,null\n"
		"union select ?,4,?,null,null\n"
		"union select ?,5,?,null,null\n"
		"union select ?,6,?,null,null\n"
		"union select ?,7,?,null,null\n"
		"union select ?,8,?,null,null\n"
		"union select ?,9,?,null,null\n"
		"union select ?,10,?,null,null\n"
		"union select ?,11,?,null,null\n"
		"union select ?,12,?,null,null\n"
		"union select ?,13,?,null,null\n",

	/* [3] INSERT_XNAME (name) */
	"insert or ignore into xdb_xname (name) values (?)\n",

	/* [4] INSERT_XDATA (gfid, name, ival, rval, sval) */
	"insert or replace into xdb_xdata (gid, nid, ival, rval, sval)\n"
		"values\n"
		"(?,(select nid from xdb_xname where name=?),?,?,?)\n",
};

static sqlite3_stmt *stmts[N_SQLS];

static char *pragma_stmt = "pragma mmap_size=1610612736;\n"
			   "pragma journal_mode=wal;\n"
			   "pragma synchronous=0;\n"
			   "pragma temp_store=2;\n";

static inline void dberror(int code)
{
	fprintf(stderr, "db error: %s, %s (ret=%d)\n",
			sqlite3_errmsg(conn),
			sqlite3_errstr(code), code);
}

static inline int exec_simple_sql(const char *sql)
{
	int ret = -EINVAL;

	if (sql) {
		ret = sqlite3_exec(conn, sql, NULL, NULL, NULL);
		if (ret != SQLITE_OK) {
			dberror(ret);
			ret = -EIO;
		}
	}

	return ret;
}

static int db_connect(void)
{
	int i = 0;
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_open(dbpath, &conn);
	if (ret != SQLITE_OK)
		goto db_error;

	ret = exec_simple_sql(pragma_stmt);
	if (ret != SQLITE_OK)
		goto db_error;

	for (i = 0; i < N_SQLS; i++) {
		ret = sqlite3_prepare_v2(conn, sqls[i], -1, &stmts[i], 0);
		if (ret != SQLITE_OK)
			goto db_error;
	}

	vmsg("connected to database (%s)\n", dbpath);

	ret = exec_simple_sql("begin transaction;");
	if (ret != SQLITE_OK)
		goto db_error;

	return 0;

db_error:
	dberror(ret);
	return -EIO;
}

static int db_disconnect(void)
{
	int i = 0;
	int ret = 0;

	if (conn) {
		for (i = 0; i < N_SQLS; i++)
			sqlite3_finalize(stmts[i]);


		ret = exec_simple_sql("end transaction;");
		if (ret != SQLITE_OK)
			goto db_error;

		sqlite3_close(conn);
	}

	vmsg("database closed\n");

	return 0;

db_error:
	dberror(ret);
	return -EIO;
}

static int read_attributes(const char *file)
{
	int i = 0;
	int ret = 0;
	ssize_t pos = 0;
	ssize_t listlen = 0;
	ssize_t valuelen = 0;

	memset(fattr, 0, sizeof(*fattr));

	if (!file)
		return -EINVAL;

	fattr->file = file;

	vmsg("processing %s\n", file);

	snprintf(fattr->gfid, 32, "%031llu", _llu(gfid_num++));
	vmsg(" - gfid is generated: %s\n", fattr->gfid);

	/* stat attributes */
	ret = lstat(file, &fattr->stat);
	if (ret)
		return ret;

	vmsg(" - stat attributes are collected\n");

	/* load extended attributes */
	listlen = llistxattr(file, xattrlist, sizeof(xattrlist));
	if (listlen < 0)
		return -1;

	for (pos = 0; pos < listlen; pos += strlen(&xattrlist[pos]) + 1) {
		struct xattr *current = &fattr->xattrs[i];

		current->name = &xattrlist[pos];
		valuelen = lgetxattr(file, current->name, current->val,
					MAX_XATTR_VAL);
		if (valuelen < 0)
			return -1;

		current->len = valuelen;
		i++;

		vmsg(" - read xattr %s\n", current->name);
	}

	fattr->n_xattrs = i;

	vmsg(" - %d xattrs are collected\n", i);

	return ret;
}

static inline int name_substr_pos(const char *path)
{
	char *pos = NULL;

	pos = rindex (path, '/');
	return (int) (((unsigned long) pos - (unsigned long) path) + 2);
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

static int update_database(void)
{
	int i = 0;
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	/* insert gfid */
	stmt = stmts[INSERT_GFID];

	ret = sqlite3_bind_text(stmt, 1, fattr->gfid, -1, SQLITE_STATIC);
	if (ret != SQLITE_OK)
		goto db_error;

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		goto db_error;

	fattr->gid = sqlite3_last_insert_rowid(conn);
	sqlite3_reset(stmt);

	vmsg(" - inserted gfid to database\n");

	/* insert file */
	stmt = stmts[INSERT_FILE];

	ret = sqlite3_bind_int(stmt, 1, fattr->gid);
	ret |= sqlite3_bind_text(stmt, 2, fattr->file, -1, SQLITE_STATIC);
	ret |= sqlite3_bind_text(stmt, 3, fattr->file, -1, SQLITE_STATIC);
	ret |= sqlite3_bind_int(stmt, 4, name_substr_pos(fattr->file));
	if (ret != SQLITE_OK)
		goto db_error;

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		goto db_error;

	fattr->fid = sqlite3_last_insert_rowid(conn);
	sqlite3_reset(stmt);

	vmsg(" - inserted file to database\n");

	/* insert stat */
	stmt = stmts[INSERT_STAT];

	ret = bind_stat(stmt, fattr->gid, &fattr->stat);
	if (ret)
		goto db_error;

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		goto db_error;

	sqlite3_reset(stmt);

	vmsg(" - inserted stat records to database\n");

	/* TODO: for now, we treat all xattrs as string values. */
	for (i = 0; i < fattr->n_xattrs; i++) {
		struct xattr *xattr = &fattr->xattrs[i];

		/* insert xattr name */
		stmt = stmts[INSERT_XNAME];

		ret = sqlite3_bind_text(stmt, 1, xattr->name, -1,
					SQLITE_STATIC);
		if (ret != SQLITE_OK)
			goto db_error;

		ret = sqlite3_step(stmt);
		if (ret != SQLITE_DONE)
			goto db_error;

		sqlite3_reset(stmt);

		/* insert xattr value */
		stmt = stmts[INSERT_XDATA];

		ret = sqlite3_bind_int(stmt, 1, fattr->gid);
		ret |= sqlite3_bind_text(stmt, 2, xattr->name, -1,
					SQLITE_STATIC);
		ret |= sqlite3_bind_null(stmt, 3);
		ret |= sqlite3_bind_null(stmt, 4);
		ret |= sqlite3_bind_text(stmt, 5, xattr->val, -1,
					SQLITE_STATIC);

		if (ret != SQLITE_OK)
			goto db_error;

		ret = sqlite3_step(stmt);
		if (ret != SQLITE_DONE)
			goto db_error;

		sqlite3_reset(stmt);
	}

	vmsg(" - inserted %d xattr records to database\n",
		fattr->n_xattrs);

	/* update statistics */
	total_files++;
	total_xattrs += fattr->n_xattrs;

	return 0;

db_error:
	dberror(ret);
	return -EIO;
}

static int do_recovery(void)
{
	int ret = 0;
	FILE *fp = NULL;
	uint64_t processed = 0;

	gettimeofday(&t_recovery_begin, NULL);

	fp = fopen(logfile, "r");
	if (!fp) {
		perror("Failed to open the input file");
		ret = -errno;
		goto out;
	}

	while (fgets(linebuf, 511, fp) != NULL) {
		if (linebuf[0] == '#')
			continue;

		linebuf[strlen(linebuf) - 1] = '\0';
		ret = read_attributes(linebuf);
		if (ret) {
			fprintf(stderr, "recover for '%s' failed: %s\n",
					linebuf, strerror(errno));
			goto out_close;
		}

		ret = update_database();
		if (ret) {
			fprintf(stderr,
				"failed to update database for file '%s':"
				"%s\n",
				linebuf, strerror(errno));
			goto out_close;

		}

		processed++;

		if (processed == count) {
			vmsg("%llu files are processed, finishing..\n",
				_llu(processed));
			goto done;
		}
	}
	if (ferror(fp)) {
		perror("Error while reading the input file");
		goto out_close;
	}

done:
	gettimeofday(&t_recovery_end, NULL);

out_close:
	fclose(fp);
out:
	return ret;
}

static struct option opts[] = {
	{ .name = "count", .has_arg = 1, .flag = NULL, .val = 'c' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "verbose", .has_arg = 0, .flag = NULL, .val = 'v' },
	{ 0, 0, 0, 0},
};

static const char *usage_str =
"\nixrecover [OPTIONS] <index database> <log file>\n"
"\n"
"OPTIONS:\n"
"--count=NUM, -c NUM    Process only first NUM entries\n"
"--help, -h             Help message.\n"
"--verbose, -v          Produce noisy output.\n\n";

static void print_usage(void)
{
	fputs(usage_str, stdout);
}

int main(int argc, char **argv)
{
	int ret = 0;
	int op = 0;

	while ((op = getopt_long(argc, argv, "c:hgv", opts, NULL)) != -1) {
		switch (op) {
		case 'c':
			count = strtoull(optarg, 0, 0);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			print_usage();
			return -1;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2) {
		print_usage();
		return -1;
	}

	dbpath = argv[0];
	logfile = argv[1];

	ret = db_connect();
	if (ret)
		return ret;

	ret = do_recovery();
	if (ret)
		perror("recovery failed");

	db_disconnect();

	printf("%llu files and %llu xattrs are updated in %.6lf seconds\n",
		_llu(total_files), _llu(total_xattrs),
		timegap_double(&t_recovery_begin, &t_recovery_end));

	return ret;
}

