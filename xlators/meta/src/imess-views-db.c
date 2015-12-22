#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <sqlite3.h>

#include "xlator.h"
#include "defaults.h"

#include "meta-mem-types.h"
#include "meta.h"
#include "meta-hooks.h"

/*
 * FIXME:
 * - currently, the database path is hard-coded.
 * - we assume that the database is already created (externally).
 */
static const char *viewdb_path = "/tmp/imess-view.db";

typedef struct {
	int count;
	dict_t *data;
} viewdb_getlist_callback_data_t;

static int
viewdb_getlist_callback (void *arg, int ncols, char **vals, char **names)
{
	int ret = 0;
	viewdb_getlist_callback_data_t *data = arg;

	data->count++;

	ret = dict_set_str (data->data,
			    gf_strdup (vals[0]), gf_strdup (vals[1]));
	return ret;
}

static int imess_viewdb_count (sqlite3 *db)
{
	int ret            = 0;
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2 (db, "select count(*) from imess_views",
					-1, &stmt, 0);
	if (ret != SQLITE_OK)
		return -1;

	ret = sqlite3_step (stmt);
	if (ret != SQLITE_ROW) {
		ret = -2;
		goto out;
	}

	ret = sqlite3_column_int (stmt, 0);
out:
	if (stmt)
		sqlite3_finalize (stmt);

	return ret;
}

int imess_viewdb_getlist (dict_t *data)
{
	int ret      = 0;
	int count    = 0;
	char *errstr = NULL;
	sqlite3 *db  = NULL;
	viewdb_getlist_callback_data_t callback_data = { 0, };

	ret = sqlite3_open (viewdb_path, &db);
	if (ret)
		return ret;

	count = imess_viewdb_count (db);
	if (count < 0) {
		ret = count;
		goto out_close;
	}

	callback_data.count = 0;
	callback_data.data = data;

	ret = sqlite3_exec (db, "select name,sql from imess_views",
			    viewdb_getlist_callback, &callback_data, &errstr);
	if (ret) {
		gf_log ("meta-imess", GF_LOG_WARNING,
			"sqlite3_exec failed (%d:%s)", ret, errstr);
		if (errstr)
			sqlite3_free (errstr);
		ret = -EIO;
	}

out_close:
	sqlite3_close (db);
	return ret;
}

