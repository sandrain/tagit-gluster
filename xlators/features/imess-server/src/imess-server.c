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
 * helper functions
 */

static int populate_xdb (ims_priv_t *priv, struct iatt *buf, const char *path)
{
        int ret              = 0;
        ims_xdb_t *xdb       = NULL;
        ims_xdb_file_t  file = { 0, };
        struct stat sb       = { 0, };

        xdb = priv->xdb;

        file.gfid = uuid_utoa (buf->ia_gfid);
        file.path = path;

        ims_xdb_tx_begin (xdb);

        ret = ims_xdb_insert_file (xdb, &file);
        if (ret)
                goto out;

        iatt_to_stat (buf, &sb);

        ret = ims_xdb_insert_stat (xdb, &file, &sb);

out:
        if (ret)
                ims_xdb_tx_rollback (xdb);
        else
                ims_xdb_tx_commit (xdb);

        return ret;
}

/*
 * xlator fops implementations
 */

/*
 * mkdir
 */

int32_t
ims_mkdir_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
               int32_t op_ret, int32_t op_errno,
               inode_t *inode, struct iatt *buf,
               struct iatt *preparent, struct iatt *postparent,
               dict_t *xdata)
{
        int ret          = 0;
        ims_priv_t *priv = NULL;

        if (op_ret == -1)
                goto out;

        priv = this->private;
        ret = populate_xdb (priv, buf, (const char *) cookie);
        if (ret)
                gf_log (this->name, GF_LOG_WARNING,
                        "ims_mkdir_cbk: populate_xdb failed "
                        "(ret=%d, db_ret=%d)",
                        ret, priv->xdb->db_ret);

out:
        if (cookie)
                GF_FREE (cookie);

        STACK_UNWIND_STRICT (mkdir, frame, op_ret, op_errno, inode, buf,
                             preparent, postparent, xdata);
        return 0;
}

int32_t
ims_mkdir (call_frame_t *frame, xlator_t *this, loc_t *loc, mode_t mode,
           mode_t umask, dict_t *xdata)
{
        void *cookie = NULL;

        cookie = gf_strdup (loc->path);

        STACK_WIND_COOKIE (frame, ims_mkdir_cbk, cookie,
                           FIRST_CHILD (this),
                           FIRST_CHILD (this)->fops->mkdir,
                           loc, mode, umask, xdata);
        return 0;
}

/*
 * create
 */

int
ims_create_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                int32_t op_ret, int32_t op_errno,
                fd_t *fd, inode_t *inode, struct iatt *buf,
                struct iatt *preparent, struct iatt *postparent,
                dict_t *xdata)
{
        int ret            = 0;
        ims_priv_t *priv = NULL;

        if (op_ret == -1)
                goto out;

        priv = this->private;

        ret = populate_xdb (priv, buf, (const char *) cookie);
        if (ret)
                gf_log (this->name, GF_LOG_WARNING,
                        "ims_create_cbk: populate_xdb failed "
                        "(ret=%d, db_ret=%d)",
                        ret, priv->xdb->db_ret);

out:
        GF_FREE(cookie);
        STACK_UNWIND_STRICT (create, frame, op_ret, op_errno, fd, inode, buf,
                             preparent, postparent, xdata);
        return 0;
}

int
ims_create (call_frame_t *frame, xlator_t *this, loc_t *loc, int32_t flags,
            mode_t mode, mode_t umask, fd_t *fd, dict_t *xdata)
{
        void *cookie = NULL;

        cookie = gf_strdup (loc->path);

        STACK_WIND_COOKIE (frame, ims_create_cbk, cookie,
                           FIRST_CHILD (this),
                           FIRST_CHILD( this)->fops->create,
                           loc, flags, mode, umask, fd, xdata);
        return 0;
}

/*
 * symlink
 */

int
ims_symlink_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                   int32_t op_ret, int32_t op_errno,
                   inode_t *inode, struct iatt *buf,
                   struct iatt *preparent, struct iatt *postparent,
                   dict_t *xdata)
{
        int ret          = 0;
        ims_priv_t *priv = NULL;

        if (op_ret == -1)
                goto out;

        priv = this->private;

        ret = populate_xdb (priv, buf, (const char *) cookie);
        if (ret)
                gf_log (this->name, GF_LOG_WARNING,
                        "ims_symlink_cbk: populate_xdb failed "
                        "(ret=%d, db_ret=%d)",
                        ret, priv->xdb->db_ret);

out:
        GF_FREE(cookie);
        STACK_UNWIND_STRICT (symlink, frame, op_ret, op_errno, inode, buf,
                             preparent, postparent, xdata);
        return 0;
}


int
ims_symlink (call_frame_t *frame, xlator_t *this, const char *linkpath,
               loc_t *loc, mode_t umask, dict_t *xdata)
{
        void *cookie = NULL;

        cookie = gf_strdup(loc->path);

        STACK_WIND_COOKIE (frame, ims_symlink_cbk, cookie,
                           FIRST_CHILD (this),
                           FIRST_CHILD (this)->fops->symlink,
                           linkpath, loc, umask, xdata);
        return 0;
}

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
	int op_ret       = -1;
        int op_errno     = 0;
        ims_priv_t *priv = NULL;
        ims_xdb_t *xdb   = NULL;
        dict_t *xdout    = NULL;
        char *sql        = NULL;

        if (op != IMESS_IPC_OP) {
                STACK_WIND (frame, ims_ipc_cbk,
                            FIRST_CHILD (this), FIRST_CHILD (this)->fops->ipc,
                            op, xdata);
                return 0;
        }

        op_ret = dict_get_str (xdata, "sql", &sql);

        if (op_ret) {
                gf_log (this->name, GF_LOG_WARNING,
                        "ims_ipc: wrong IPC request: no sql in xdata.\n");
                op_errno = EINVAL;
                goto out;
        }

        xdout = dict_new ();
        if (xdout == NULL) {
                gf_log (this->name, GF_LOG_WARNING,
                        "ims_ipc: dict_new() failed.\n");
                op_errno = ENOMEM;
                goto out;
        }

        priv = this->private;
        xdb = priv->xdb;

        op_ret = ims_xdb_direct_query (xdb, sql, xdout);
        if (op_ret) {
                gf_log (this->name, GF_LOG_WARNING,
                        "ims_ipc: direct query failed: db_ret=%d (%s).\n",
                        xdb->db_ret, ims_xdb_errstr (xdb->db_ret));
                op_errno = EIO;
        }

out:
        STACK_UNWIND_STRICT (ipc, frame, op_ret, op_errno, xdout);

        if (xdout)
                dict_unref (xdout);
        return 0;
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

	ret = ims_xdb_init (&priv->xdb, priv->db_path);
	if (ret) {
		gf_log (this->name, GF_LOG_ERROR,
			"FATAL: index database intialization failed: "
			"(db_ret = %d)", priv->xdb->db_ret);
		goto out;
	}

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
        .mkdir        = ims_mkdir,
        .symlink      = ims_symlink,
        .create       = ims_create,
	.ipc          = ims_ipc,
};

#if 0
struct xlator_fops fops = {
        .stat         = ims_stat,
        .readlink     = ims_readlink,
        .mknod        = ims_mknod,
        .mkdir        = ims_mkdir,
        .unlink       = ims_unlink,
        .rmdir        = ims_rmdir,
        .symlink      = ims_symlink,
        .rename       = ims_rename,
        .link         = ims_link,
        .truncate     = ims_truncate,
        .open         = ims_open,
        .readv        = ims_readv,
        .writev       = ims_writev,
        .statfs       = ims_statfs,
        .flush        = ims_flush,
        .fsync        = ims_fsync,
        .setxattr     = ims_setxattr,
        .getxattr     = ims_getxattr,
        .removexattr  = ims_removexattr,
        .fsetxattr    = ims_fsetxattr,
        .fgetxattr    = ims_fgetxattr,
        .fremovexattr = ims_fremovexattr,
        .opendir      = ims_opendir,
        .readdir      = ims_readdir,
        .readdirp     = ims_readdirp,
        .fsyncdir     = ims_fsyncdir,
        .access       = ims_access,
        .ftruncate    = ims_ftruncate,
        .fstat        = ims_fstat,
        .create       = ims_create,
        .lk           = ims_lk,
        .inodelk      = ims_inodelk,
        .finodelk     = ims_finodelk,
        .entrylk      = ims_entrylk,
        .lookup       = ims_lookup,
        .xattrop      = ims_xattrop,
        .fxattrop     = ims_fxattrop,
        .setattr      = ims_setattr,
        .fsetattr     = ims_fsetattr,
	.fallocate    = ims_fallocate,
	.discard      = ims_discard,
        .zerofill     = ims_zerofill,
	.ipc          = ims_ipc,
};
#endif

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
