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

/*
 * execute the command with each file and return any output via xdata.
 */
static int32_t
ipc_active_work_files_exec (xlator_t *this, char *operator, dict_t *xfiles,
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
				"ims_ipc_exec_work_files: "
				"failed to execute %s (%d: %s)",
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
 * execute the command with each file and return any output via xdata.
 */

#if 0
static inline int32_t
ipc_extractor_handle_xattr(xlator_t *this, ims_priv_t *priv,
				const char *path, dict_t *xattrs)
{
	/* can we directly call the ims_setxattr here? or need to call
	 * syncop_setxattr ? */

	return 0;
}
#endif

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

static int32_t
extractor_put_xdb_task (xlator_t *this, loc_t *loc,
			const char *path, dict_t *dict, int *err)
{
	int op_ret                = -1;
	int op_errno              = 0;
	ims_priv_t *priv          = NULL;
	ims_xdb_t *xdb            = NULL;
	ims_xdb_attr_t xattr      = { 0, };
	ims_xattr_filler_t filler = { 0, };
	ims_task_t task           = { {0,0}, };

	priv = this->private;
	xdb = priv->xdb;

	filler.xattr = &xattr;
	op_ret = dict_foreach (dict, ims_find_setxattr_kv, &filler);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"extractor_put_xdb_task: ims_find_xattr_kv failed"
			" (%d)", op_ret);
		goto out;
	}

	if (filler.count != 1) {
		gf_log (this->name, GF_LOG_WARNING,
			"extractor_put_xdb_task: ims_find_setxattr_kv counts"
			" %d xattrs", filler.count);
		goto out;
	}

	xattr.gfid = uuid_utoa (loc->inode->gfid);

	task.op = IMS_TASK_INSERT_XATTR;
	task.attr = xattr;

	op_ret = ims_async_put_task (priv->async_ctx, &task);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"extractor_put_xdb_task: ims_async_put_task failed"
			" (%d)", op_ret);
	}

out:
	*err = op_errno;
	return op_ret;
}

static inline int32_t
extractor_setxattr (xlator_t *this, const char *path, char *line, int *err)
{
	int op_ret          = -1;
	int op_errno        = 0;
	int flags           = 0;
	loc_t loc           = { 0, };
	dict_t *xattr       = NULL;

	op_ret = parse_line_to_xattr (line, &xattr, &op_errno);
	if (op_ret)
		goto out;

	/*
	 * FIXME: syncop_XXX is not working here. plus, the client cannot set
	 * it because the target files should be searched from here.
	 *
	 * so, probably, we need to use sys_lsetxattr directly, with converting
	 * the path into the realpath as the posix xlator does.
	 */

	op_ret = syncop_setxattr (FIRST_CHILD (this),
				  &loc, xattr, flags, NULL, NULL);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"extractor_setxattr: syncop_setxattr failed (%d)",
			op_ret);
		goto out;
	}

	/*
	 * FIXME: here as well, we cannot use the gfid in the loc_t, which is
	 * empty.
	 */

	op_ret = extractor_put_xdb_task (this, &loc, path, xattr, err);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"extractor_setxattr: ims_put_xdb_task failed (%d)",
			op_ret);
	}

out:
	*err = op_errno;
	return op_ret;
}

static int32_t
ipc_active_work_files_extract (xlator_t *this, char *operator, dict_t *xfiles,
				dict_t *result, int *err)
{
	int op_ret             = -1;
	uint64_t i             = 0;
	uint64_t n_files       = 0;
	uint64_t count         = 0;
	char *file             = NULL;
	char keybuf[12]        = { 0, };
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
				"ims_ipc_exec_work_files: "
				"failed to execute %s (%d: %s)",
				operator, errno, strerror (errno));
			continue;
		}

		while ((ptr = fgets (linebuf, sizeof(linebuf),
				     runner_chio (&runner, STDOUT_FILENO)))
			!= NULL)
		{
			op_ret = extractor_setxattr (this, path, ptr, err);
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


int32_t
ims_ipc_active_exec (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out,
			int *err, int type)
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
			"ims_ipc_exec: dict_new () failed.\n");
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

	switch (type) {
	case IMS_IPC_TYPE_EXEC:
		op_ret = ipc_active_work_files_exec (this, operator, xfiles,
							xdata_out, &op_errno);
		break;
	case IMS_IPC_TYPE_EXTRACTOR:
		op_ret = ipc_active_work_files_extract (this, operator, xfiles,
							xdata_out, &op_errno);
		break;
	case IMS_IPC_TYPE_FILTER:
		/* TODO: not implemented, yet */
		break;
	default:
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_active_exec: unknown request type (%d).",
			type);
		op_errno = EINVAL;
		goto out;
	}
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

#if 0
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
			"ims_ipc_exec: dict_new () failed.\n");
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
#endif

