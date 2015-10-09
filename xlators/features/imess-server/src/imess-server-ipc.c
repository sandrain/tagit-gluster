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
#include <string.h>
#include <ctype.h>

#include "glusterfs.h"
#include "glusterfs-acl.h"
#include "xlator.h"
#include "logging.h"
#include "syncop.h"
#include "run.h"

#include "imess-server.h"

#ifndef _llu
#define _llu(x)	((unsigned long long) (x))
#endif

#if 0
static inline int32_t
parse_line_to_xattr (char *line, dict_t **xattr, int *err)
{
	int op_ret = -1;
	int op_err = 0;
	char *key  = NULL;
	char *val  = NULL;
	dict_t *xa = NULL;

	key = line;
	val = strchr (line, '=');
	if (val == NULL) {
		op_err = EINVAL;
		goto out;
	}

	*val++ = '\0';
	if (!val) {
		op_err = EINVAL;
		goto out;
	}

	xa = dict_for_key_value (key, val, strlen (val));
	if (!xa) {
		op_err = ENOMEM;
		goto out;
	}

	*xattr = xa;
	op_ret = 0;
out:
	*err = op_err;
	return op_ret;
}

static inline int32_t
extractor_setxattr (xlator_t *this, const char *path, char *line, int *err)
{
	int op_ret          = -1;
	int op_errno        = 0;
	int flags           = 0;
	ims_priv_t *priv    = NULL;
	ims_xdb_t *xdb      = NULL;
	ims_xdb_file_t file = { 0, };
	ims_xdb_attr_t attr = { 0, };
	loc_t loc           = { 0, };
	dict_t *xattr       = NULL;

	priv = this->private;
	xdb = priv->xdb;

	file.path = path;

	op_ret = parse_line_to_xattr (line, &xattr, &op_errno);
	if (op_ret)
		goto out;

	op_ret = syncop_setxattr (FIRST_CHILD (this),
				  &loc, xattr, flags, NULL, NULL);

	op_ret = ims_xdb_insert_xattr (xdb, &file, &attr, 1);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipcc_extractor: ims_xdb_insert_xattr failed "
			"(db_ret = %d, %s).",
			xdb->db_ret, ims_xdb_errstr (xdb->db_ret));
		goto out;
	}

out:
	*err = op_errno;
	return op_ret;
}
#endif

/*
 * execute the command with each file, and set attributes accordingly.
 */
static int32_t
ipc_exec_work_files (xlator_t *this, char *operator, dict_t *xfiles,
		     dict_t *result, int *err)
{
	int op_ret             = -1;
	uint64_t i             = 0;
	uint64_t n_files       = 0;
	uint64_t count         = 0;
	char *file             = NULL;
	char keybuf[12]        = { 0, };
	char rkeybuf[12]       = { 0, };
	runner_t runner        = { 0, };
	char path[PATH_MAX]    = { 0, };
	char linebuf[PATH_MAX] = { 0, };
	char *ptr              = NULL;
	ims_priv_t *priv       = NULL;

	priv = this->private;
	op_ret = dict_get_uint64 (xfiles, "count", &n_files);

	for (i = 0; i < n_files; i++) {
		sprintf (keybuf, "%llu", _llu (i));

		op_ret = dict_get_str (xfiles, keybuf, &file);
		sprintf (path, "%s%s", priv->brick_path, file);

		runinit (&runner);
		runner_add_args (&runner, operator, path, NULL);
		runner_redir (&runner, STDOUT_FILENO, RUN_PIPE);
		op_ret = runner_start (&runner);
		if (op_ret == -1) {
			gf_log (this->name, GF_LOG_WARNING,
				"ims_ipc_exec: failed to execute %s (%d: %s)",
				operator, errno, strerror (errno));
			continue;
		}

		while ((ptr = fgets (linebuf, sizeof(linebuf),
				     runner_chio (&runner, STDOUT_FILENO)))
			!= NULL)
		{
			sprintf (rkeybuf, "%llu", _llu (count));
			op_ret = dict_set_dynstr_with_alloc (result, rkeybuf,
							     ptr);
			if (op_ret)
				goto close;
			count++;
		}
close:
		op_ret = runner_end (&runner);
		if (op_ret)
			gf_log (this->name, GF_LOG_WARNING,
				"ims_ipc_exec: runner_end failed (%d: %s)",
				errno, strerror (errno));
	}

	op_ret  = dict_set_int32 (result, "ret", op_ret);
	op_ret |= dict_set_uint64 (result, "count", count);

	if (op_ret)
		*err = errno;

	return op_ret;
}

/*
 * API
 */

int32_t
ims_ipc_query (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	int op_ret             = -1;
	int op_errno           = 0;
	char *sql              = NULL;
	ims_priv_t *priv       = NULL;
	ims_xdb_t *xdb         = NULL;
	struct timeval before  = { 0, };
	struct timeval after   = { 0, };
	double latency         = .0F;

	priv = this->private;
	xdb = priv->xdb;

	op_ret = dict_get_str (xdata_in, "sql", &sql);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: IPC query request: no sql in xdata.");
		op_errno = EINVAL;
		goto out;
	}

	gettimeofday (&before, NULL);

	op_ret = ims_xdb_direct_query (xdb, sql, xdata_out);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: direct query failed: db_ret=%d (%s).",
			xdb->db_ret, ims_xdb_errstr (xdb->db_ret));
		op_errno = EIO;
	}

	gettimeofday (&after, NULL);
	latency = timegap_double (&before, &after);
	op_ret = dict_set_double (xdata_out, "runtime", latency);

out:
	*err = op_errno;
	return op_ret;
}

int32_t
ims_ipc_exec (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	int op_ret             = -1;
	int op_errno           = 0;
	char *sql              = NULL;
	char *operator         = NULL;
	uint64_t count         = 0;
	ims_priv_t *priv       = NULL;
	ims_xdb_t *xdb         = NULL;
	dict_t *xfiles         = NULL;
	struct timeval before  = { 0, };
	struct timeval after   = { 0, };
	double latency         = .0F;

	priv = this->private;
	xdb = priv->xdb;

	op_ret = dict_get_str (xdata_in, "sql", &sql);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_exec: IPC extractor request: "
			"no sql in xdata.");
		op_errno = EINVAL;
		goto out;
	}

	op_ret = dict_get_str (xdata_in, "operator", &operator);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_exec: IPC extractor request: "
			"no operator in xdata.");
		op_errno = EINVAL;
		goto out;
	}

	xfiles = dict_new ();
	if (xfiles == NULL) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_extractor: dict_new () failed.\n");
		op_errno = ENOMEM;
		goto out;
	}

	/*
	 * get the list of files that we need to work with
	 */
	op_ret = ims_xdb_direct_query (xdb, sql, xfiles);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_exec: query failed: db_ret=%d (%s).",
			xdb->db_ret, ims_xdb_errstr (xdb->db_ret));
		op_errno = EIO;
		goto out;
	}

	op_ret = dict_get_uint64 (xfiles, "count", &count);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_exec: dict_get_uint64 failed.");
		op_errno = EINVAL;
		goto out;
	}

	if (!count) {
		op_ret = dict_set_int32 (xdata_out, "ret", 0);
		op_ret = dict_set_uint64 (xdata_out, "count", 0);

		goto done;
	}

	/*
	 * TODO: async worker??
	 * currently, we work synchronously.
	 */

	gettimeofday (&before, NULL);

	op_ret = ipc_exec_work_files (this, operator, xfiles,
				      xdata_out, &op_errno);
	if (op_ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_exec: execution failed (ret=%d, errno=%d)",
			op_ret, op_errno);

	gettimeofday (&after, NULL);

	latency = timegap_double (&before, &after);
done:
	op_ret = dict_set_double (xdata_out, "runtime", latency);
out:
	*err = op_errno;
	if (xfiles)
		dict_unref (xfiles);
	return op_ret;
}


int32_t
ims_ipc_filter (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	return 0;
}

int32_t
ims_ipc_extractor (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	return 0;
}

