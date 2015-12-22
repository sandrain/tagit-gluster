/* Copyright (C) 2015 	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <sqlite3.h>

/*
 * from imess-view-db-schema.c
 */
extern const char *view_db_schema_sqlstr;

/*
 * database connection
 */
static const char *dbfile = "/tmp/imess-view.db";
static sqlite3 *dbconn;

static const char *sql_table_exist = 
	"select count(type) from sqlite_master\n"
	"where type='table' and name='imess_views'\n";
static const char *sql_insert = 
	"insert into imess_views (name, sql) values (?,?)\n";

static int viewdb_init (void)
{
	int ret = 0;
	int n_tables = 0;
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_open (dbfile, &dbconn);
	if (ret != SQLITE_OK)
		goto dberr;

	/* check weather the table is created. */
	ret = sqlite3_prepare_v2 (dbconn, sql_table_exist, -1, &stmt, 0);
	if (ret != SQLITE_OK)
		goto dberr;

	ret = sqlite3_step (stmt);
	if (ret != SQLITE_ROW)
		goto dberr;

	n_tables = sqlite3_column_int (stmt, 0);
	if (n_tables == 1)
		return 0;

	/* create the table. */
	ret = sqlite3_exec (dbconn, view_db_schema_sqlstr, 0, 0, 0);
	if (ret != SQLITE_OK)
		goto dberr;

	return 0;

dberr:
	fprintf (stderr, "database interaction failed: %s\n",
			sqlite3_errstr (ret));
	return -1;
}

static void viewdb_exit (void)
{
	if (dbconn)
		sqlite3_close (dbconn);
}

/* 
 * no need to protect with transaction. without using transactions, sqlite will
 * by default create a transaction for the insert.
 */
static int viewdb_append (const char *name, const char *sql)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2 (dbconn, sql_insert, -1, &stmt, 0);
	if (ret != SQLITE_OK)
		goto dberr;

	ret = sqlite3_bind_text (stmt, 1, name, -1, SQLITE_STATIC);
	if (ret != SQLITE_OK)
		goto dberr;
	ret = sqlite3_bind_text (stmt, 2, sql, -1, SQLITE_STATIC);
	if (ret != SQLITE_OK)
		goto dberr;

	do {
		ret = sqlite3_step (stmt);
	} while (ret == SQLITE_BUSY);

	sqlite3_finalize (stmt);

	if (ret == SQLITE_DONE)
		return 0;

dberr:
	fprintf (stderr, "database interaction failed: %s\n",
			sqlite3_errstr (ret));
	return -1;
}

/*
 * main program
 */
static struct option opts[] = {
	{ .name = "help", .has_arg = 0, .flag = 0, .val = 'h' },
	{ 0, 0, 0, 0 },
};

static const char *usage_str = 
"\n"
"usage: ixview [options..] <view name> <associated SQL>\n"
"options:\n"
"  --help, -h                print this help message\n"
"\n\n";

static inline void print_usage (void)
{
	fputs(usage_str, stdout);
}

int main (int argc, char **argv)
{
	int ret = 0;
	int op  = 0;
	char *view_name = NULL;
	char *view_sql = NULL;

	while ((op = getopt_long (argc, argv, "h", opts, 0)) != -1) {
		switch (op) {
		case 'h':
		default:
			print_usage ();
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2) {
		print_usage ();
		return 1;
	}

	view_name = argv[0];
	view_sql = argv[1];

	if ((ret = viewdb_init ()) != 0) {
		perror("viewdb_init() failed");
		return -1;
	}

	if ((ret = viewdb_append (view_name, view_sql)) != 0)
		perror("viewdb_append() failed");

	viewdb_exit ();

	return ret;
}

