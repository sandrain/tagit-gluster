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

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"

#include "imess-server.h"

/*
 * ipc
 */

int32_t
ims_ipc_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
	     int32_t op_ret, int32_t op_errno, dict_t *xdata)
{
	int ret = 0;

	STACK_UNWIND_STRICT (ipc, frame, op_ret, op_errno, xdata);

	return ret;
}

int32_t
ims_ipc (call_frame_t *frame, xlator_t *this, int op, dict_t *xdata)
{
	int ret = 0;

	STACK_WIND (frame, ims_ipc_cbk,
		    FIRST_CHILD (this), FIRST_CHILD (this)->fops->ipc,
		    op, xdata);
	return ret;
}

/*
 * xlator registration
 */

int32_t
mem_acct_init (xlator_t *this)
{
	int ret = -1;

	ret = xlator_mem_acct_init (this, gf_ims_mt_end + 1);
	if (ret)
		gf_log (this->name, GF_LOG_ERROR,
			"Memory accounting initialization failed.");

	return ret;
}

int32_t
init (xlator_t *this)
{
	int ret            = -1;
	dict_t *options    = NULL;
	ims_priv_t *priv   = NULL;

	GF_VALIDATE_OR_GOTO ("imess-server", this, out);

        if (!this->children || this->children->next) {
                gf_log (this->name, GF_LOG_ERROR,
                        "FATAL: imess server should have exactly one child");
                goto out;
        }

	if (!this->parents) {
		gf_log (this->name, GF_LOG_WARNING,
			"dangling volume. check volfile");
	}

	priv = GF_CALLOC (1, sizeof (*priv), gf_ims_mt_ims_priv_t);
	if (!priv)
		goto out;

	options = this->options;

	GF_OPTION_INIT ("db-path", priv->db_path, str, out);
	GF_OPTION_INIT ("enable-lookup-cache", priv->lookup_cache,
			bool, out);

	gf_log (this->name, GF_LOG_INFO,
		"imess-server initialized: db-path=%s, lookup-cache=%d",
		priv->db_path, priv->lookup_cache);
	ret = 0;

out:
	if (ret < 0) {
		if (priv)
			GF_FREE (priv);
	}

	this->private = priv;

	return ret;
}

void
fini (xlator_t *this)
{
	ims_priv_t *priv = NULL;

	priv = this->private;

	if (priv)
		GF_FREE (priv);

	return;
}

int32_t
ims_release (xlator_t *this, fd_t *fd)
{
        return 0;
}

int32_t
ims_releasedir (xlator_t *this, fd_t *fd)
{
        return 0;
}

int32_t
ims_forget (xlator_t *this, inode_t *inode)
{
        return 0;
}

struct xlator_cbks cbks = {
	.release    = ims_release,
	.releasedir = ims_releasedir,
	.forget     = ims_forget,
};

struct xlator_fops fops = {
	.ipc = ims_ipc,
};

struct volume_options options [] = {
	{ .key = { "db-path" },
	  .type = GF_OPTION_TYPE_PATH,
	},
	{ .key = { "enable-lookup-cache" },
	  .type = GF_OPTION_TYPE_BOOL,
	  .default_value = "off",
	  .description = "Turn on the lookup cache to speed up.",
	},
	{ .key = { NULL } },
};
