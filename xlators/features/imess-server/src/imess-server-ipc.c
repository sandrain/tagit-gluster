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
#include "syscall.h"
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

static inline int32_t
parse_line_to_xattr (char *line, ims_xdb_attr_t *xattr, int *err)
{
	int op_ret              = -1;
	int op_err              = 0;
	int type                = 0;
	int64_t ival            = 0;
	double rval             = .0f;
	char *key               = NULL;
	char *val               = NULL;
	char data_str[PATH_MAX] = { 0, };

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

	sprintf (data_str, "%s", val);

	if (strchr (data_str, '.'))
		goto try_real;

	op_ret = sscanf (data_str, "%ld", &ival);
	if (op_ret == 1) {
		type = IMS_XDB_TYPE_INTEGER;
		goto done;
	}

try_real:
	op_ret = sscanf (data_str, "%lf", &rval);
	if (op_ret == 1)
		type = IMS_XDB_TYPE_REAL;
	else
		type = IMS_XDB_TYPE_STRING;

done:
	xattr->name = key;
	xattr->type = type;
	xattr->ival = 0;
	xattr->rval = .0f;
	xattr->sval = NULL;


	switch (type) {
	case IMS_XDB_TYPE_INTEGER:
		xattr->ival = ival;
		break;
	case IMS_XDB_TYPE_REAL:
		xattr->rval = rval;
		break;
	case IMS_XDB_TYPE_STRING:
		xattr->sval = gf_strdup (data_str);
		break;
	default:
		break;
	}

	op_ret = 0;
out:
	*err = op_err;
	return op_ret;
}

/*
 * assume that only a single xattr is passed.
 */
static int
ims_sys_setxattr (ims_priv_t *priv, const char *path, ims_xdb_attr_t *xattr)
{
	int op_ret = -1;
	char value[PATH_MAX] = { 0, };
	size_t size = -1;
	ssize_t len = 0;

	if (xattr->type == IMS_XDB_TYPE_INTEGER)
		sprintf (value, "%ld", xattr->ival);
	else if (xattr->type == IMS_XDB_TYPE_REAL)
		sprintf (value, "%lf", xattr->rval);
	else if (xattr->type == IMS_XDB_TYPE_STRING)
		sprintf (value, "%s", xattr->sval);
	else
		goto out;

	/* FIXME: what is the right way to handle this? */
	size = strlen (value);
	if (xattr->type == IMS_XDB_TYPE_STRING)
		size -= 1;

	op_ret = sys_lsetxattr (path, xattr->name, value, size,
				XATTR_CREATE);
	if (op_ret == -1 && errno == EEXIST) {
		/* if already exists, replace it with a new value */
		op_ret = sys_lsetxattr (path, xattr->name, value, size,
					XATTR_REPLACE);
		if (op_ret)
			goto out;
	}

	/*
	 * now read the gfid from the file.
	 */
	memset (value, 0, sizeof(value));
	len = sys_lgetxattr (path, "trusted.gfid", (void *) value, PATH_MAX);

	xattr->gfid = uuid_utoa ((unsigned char *) value);
out:
	return op_ret;
}

static inline int32_t
extractor_setxattr (xlator_t *this, const char *path, char *line, int *err)
{
	int op_ret             = -1;
	int op_errno           = 0;
	ims_priv_t *priv       = NULL;
	ims_xdb_attr_t xattr   = { 0, };
	ims_task_t task        = { {0,0}, };

	memset (&xattr, 0, sizeof(xattr));

	op_ret = parse_line_to_xattr (line, &xattr, &op_errno);
	if (op_ret)
		goto out;

	/*
	 * FIXME: syncop_XXX is not working here. plus, the client cannot set
	 * it because the target files should be searched from here.
	 *
	 * so, probably, we need to use sys_lsetxattr directly, with converting
	 * the path into the realpath as the posix xlator does.
	 * Also, we can directly read the 'trusted.gfid' from the file.
	 */

	priv = this->private;

	op_ret = ims_sys_setxattr (priv, path, &xattr);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"extractor_setxattr: ims_sys_setxattr failed (%d, %d)",
			op_ret, errno);
		op_errno = errno;
		goto out;
	}

	/*
	 * update the database. all required fields in xattr should be set here.
	 */
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
ims_ipc_query_stateless (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out,
		         int *err)
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

static int32_t
ims_ipc_new_query (xlator_t *this, dict_t *xdata_in, dict_t *qres, int *err)
{
	int ret                = -1;
	int op_errno           = 0;
	char *sql              = NULL;
	char *session          = NULL;
	ims_priv_t *priv       = NULL;
	ims_xdb_t *xdb         = NULL;
	struct timeval before  = { 0, };
	struct timeval after   = { 0, };
	double latency         = .0F;

	priv = this->private;
	xdb = priv->xdb;

	ret = dict_get_str (xdata_in, "sql", &sql);
	ret |= dict_get_str (xdata_in, "session", &session);
	if (ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: no sql/session in xdata.");
		op_errno = EINVAL;
		goto out;
	}

	gettimeofday (&before, NULL);

	ret = ims_xdb_direct_query (xdb, sql, qres);
	if (ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: direct query failed: db_ret=%d (%s).",
			xdb->db_ret, ims_xdb_errstr (xdb->db_ret));
		op_errno = EIO;
		goto out;
	}

	gettimeofday (&after, NULL);
	latency = timegap_double (&before, &after);

	ret = dict_set_double (qres, "dbtime", latency);
	if (ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: dict_set failed: ret=%d.",
			ret);
		op_errno = EIO;
		goto out;
	}

	ret = ims_xdb_register_result (xdb, session, qres);
	if (ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_new_query: ims_xdb_register_result failed: "
			"ret=%d.\n", ret);
		op_errno = EIO;
		goto out;
	}

out:
	*err = op_errno;
	return ret;
}

static int
ims_ipc_process_query_result (ims_xdb_t *xdb, dict_t *qres, dict_t *xdata,
			      uint64_t offset, uint64_t count)
{
	int ret = 0;
	uint64_t i = 0;
	uint64_t total = 0;
	uint64_t current = 0;
	int32_t comeback = 1;
	double dbtime = 0.0F;
	char *row = NULL;
	char key[20] = { 0, };

	ret = dict_get_uint64 (qres, "count", &total);
	if (ret)
		goto out;

	if (offset == 0) {
		/* include the database latency for the first fetch */
		ret = dict_get_double (qres, "dbtime", &dbtime);
		if (ret)
			goto out;

		ret = dict_set_double (xdata, "dbtime", dbtime);
		if (ret)
			goto out;
	}

	for (i = 0; i < count; i++) {
		current = offset + i;
		sprintf(key, "%llu", _llu(current));
		ret = dict_get_str (qres, key, &row);
		if (ret)
			break;

		sprintf(key, "%llu", _llu(i));
		ret |= dict_set_dynstr (xdata, key, row);
		if (ret)
			goto out;
	}

	if (offset + i >= total) {	/* all results are processed */
		dict_del (xdb->qrset, key);
		comeback = 0;
	}

	ret = dict_set_uint64 (xdata, "count", i);
	ret |= dict_set_int32 (xdata, "ret", ret);
	ret |= dict_set_int32 (xdata, "comeback", comeback);
out:
	return ret;
}

int32_t
ims_ipc_query (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	int op_ret             = -1;
	int op_errno           = 0;
	uint64_t offset        = 0;
	uint64_t count         = 0;
	char *session          = NULL;
	ims_priv_t *priv       = NULL;
	ims_xdb_t *xdb         = NULL;
	dict_t *query_result   = NULL;
	struct timeval before  = { 0, };
	struct timeval after   = { 0, };
	double latency         = .0F;

	priv = this->private;
	xdb = priv->xdb;

	gettimeofday(&before, NULL);

	op_ret = dict_get_str (xdata_in, "session", &session);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_query: @session not found.\n");
		op_errno = EINVAL;
		goto out;
	}

	op_ret = dict_get_uint64 (xdata_in, "offset", &offset);
	op_ret |= dict_get_uint64 (xdata_in, "count", &count);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_query: no @offset/@count in xdata.");
		op_errno = EINVAL;
		goto out;
	}

	query_result = ims_xdb_get_result (xdb, session);
	if (!query_result) {
		query_result = dict_new();
		if (query_result == NULL) {
			gf_log (this->name, GF_LOG_WARNING,
				"ims_ipc_query: dict_new () failed.\n");
			op_errno = ENOMEM;
			goto out;
		}

		op_ret = ims_ipc_new_query (this, xdata_in, query_result, err);
		if (op_ret) {
			gf_log (this->name, GF_LOG_WARNING,
				"ims_ipc_new_query failed: ret=%d.\n",
				op_ret);
			goto out;
		}
	}

	op_ret = ims_ipc_process_query_result (xdb, query_result, xdata_out,
					       offset, count);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc_query: ims_ipc_process_query_result failed.");
		goto out;
	}

	gettimeofday(&after, NULL);
	latency = timegap_double (&before, &after);

	op_ret = dict_set_double (xdata_out, "runtime", latency);

out:
	*err = op_errno;
	return op_ret;
}

