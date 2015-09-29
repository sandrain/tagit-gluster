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

#include <ctype.h>

#include "glusterfs.h"
#include "glusterfs-acl.h"
#include "xlator.h"
#include "logging.h"

#include "imess-server.h"

/*
 * API
 */

int32_t
ims_ipc_query (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	int op_ret       = -1;
	int op_errno     = 0;
	char *sql        = NULL;
	ims_priv_t *priv = NULL;

	priv = this->private;

	op_ret = dict_get_str (xdata_in, "sql", &sql);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: IPC query request: no sql in xdata.");
		op_errno = EINVAL;
		goto out;
	}

	op_ret = ims_xdb_direct_query (priv->xdb, sql, xdata_out);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: direct query failed: db_ret=%d (%s).",
			priv->xdb->db_ret, ims_xdb_errstr (priv->xdb->db_ret));
		op_errno = EIO;
	}

out:
	*err = op_errno;
	if (sql)
		GF_FREE (sql);
	return op_ret;
}

int32_t
ims_ipc_filter (xlator_t *this, dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	return 0;
}

int32_t
ims_ipc_extractor (xlator_t *this,
		   dict_t *xdata_in, dict_t *xdata_out, int *err)
{
	return 0;
}
