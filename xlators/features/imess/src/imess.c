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

#include "imess.h"

/*
 * healer functions.
 */

static inline int
put_stat_attr(imess_priv_t *priv, struct iatt *buf, const char *path)
{
	int ret = 0;
	xdb_t *xdb = NULL;
	xdb_file_t file = { 0, };
	struct stat sb = { 0, };

	xdb = priv->xdb;

	file.gfid = uuid_utoa(buf->ia_gfid);
	file.path = path;

	if (path) {
		ret = xdb_insert_file(xdb, &file);
		if (ret) {
			gf_log("imess", GF_LOG_ERROR,
			       "put_stat_attr: xdb_insert_file failed (%s, %s)",
			       path, xdb->err);
			goto out;
		}
	}

	iatt_to_stat(buf, &sb);

	ret = xdb_insert_stat(xdb, &file, &sb);
	if (ret)
		gf_log("imess", GF_LOG_ERROR,
		       "put_stat_attr: xdb_insert_stat failed (%s)",
		       xdb->err);

out:
	return ret;
}

/*
 * xlator callbacks.
 */

int
imess_stat_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		int32_t op_ret, int32_t op_errno, struct iatt *buf,
		dict_t *xdata)
{
	IMESS_STACK_UNWIND(stat, frame, op_ret, op_errno, buf, xdata);
	return 0;
}

int
imess_mkdir_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int32_t op_ret, int32_t op_errno,
                 inode_t *inode, struct iatt *buf,
                 struct iatt *preparent, struct iatt *postparent,
		 dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;

	priv = this->private;

	if (op_errno)
		goto out;

	ret = put_stat_attr(priv, buf, (const char *) cookie);
	if (ret)
		gf_log(this->name, GF_LOG_ERROR, "imess_mkdir_cbk: "
				"xdb_insert_stat failed");

out:
	GF_FREE(cookie);
        IMESS_STACK_UNWIND (mkdir, frame, op_ret, op_errno, inode, buf,
                            preparent, postparent, xdata);
	return 0;
}


int
imess_create_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                  int32_t op_ret, int32_t op_errno, fd_t *fd,
                  inode_t *inode, struct iatt *buf,
                  struct iatt *preparent, struct iatt *postparent,
                  dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;

	priv = this->private;

	if (op_errno)
		goto out;

	ret = put_stat_attr(priv, buf, (const char *) cookie);
	if (ret)
		gf_log(this->name, GF_LOG_ERROR, "imess_create_cbk: "
				"xdb_insert_stat failed");

out:
	GF_FREE(cookie);
        IMESS_STACK_UNWIND (create, frame, op_ret, op_errno, fd, inode, buf,
                            preparent, postparent, xdata);
	return 0;
}

int
imess_writev_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                  int32_t op_ret, int32_t op_errno,
                  struct iatt *prebuf, struct iatt *postbuf, dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;

	priv = this->private;

	if (op_errno)
		goto out;

#if 0
	ret = put_stat_attr(priv, postbuf, NULL);
	if (ret)
		gf_log(this->name, GF_LOG_ERROR, "imess_writev_cbk: "
				"xdb_insert_stat failed");
#endif

out:
        IMESS_STACK_UNWIND (writev, frame, op_ret, op_errno, prebuf, postbuf,
                            xdata);
        return 0;
}

int
imess_lookup_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                  int32_t op_ret, int32_t op_errno,
                  inode_t *inode, struct iatt *buf,
                  dict_t *xdata, struct iatt *postparent)
{
        IMESS_STACK_UNWIND (lookup, frame, op_ret, op_errno, inode, buf,
                            xdata, postparent);
	return 0;
}

/*
 * xlator file operations.
 */

int32_t
imess_lookup (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;
	xdb_t *xdb = NULL;

	priv = this->private;

#if 0
	gf_log (this->name, GF_LOG_INFO, "imess lookup: %s", loc->path);
#endif

	if (strcmp(loc->path, "/imessmeasure"))
		goto pass;

	xdb = priv->xdb;

	gf_log (this->name, GF_LOG_INFO, "imess lookup: sending query");
#if 0
	ret = xdb_measure(xdb, "select gfid,path from xdb_file where fid in "
                               "(select fid from xdb_xdata "
			       "where aid=9 and ival=1919191919)");
#endif
	/* this will be slower, not using the index? */
	ret = xdb_measure(xdb, "select gfid,path from xdb_file "
			       "where path like '%never-existing-file.nono'");
	if (ret)
		gf_log (this->name, GF_LOG_ERROR, "xdb_measure failed.");
	else
		gf_log (this->name, GF_LOG_INFO, "query successfully finished");

	/* This line gives 10s synchronous delay to the: touch imessmeasure.
	sleep(10);
	*/

pass:
        STACK_WIND (frame, imess_lookup_cbk,
                    FIRST_CHILD(this),
                    FIRST_CHILD(this)->fops->lookup,
                    loc, xdata);

        return 0;
}

int
imess_stat (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
	imess_priv_t *priv = NULL;

	priv = this->private;

	if (!priv->enable_metacache)
		goto call_child;

	/* TODO: implement the cache logic (key: loc->inode->gfid) */

call_child:
	STACK_WIND (frame, imess_stat_cbk, FIRST_CHILD(this),
		    FIRST_CHILD(this)->fops->stat,
		    loc, xdata);
	return 0;
}

int
imess_mkdir (call_frame_t *frame, xlator_t *this, loc_t *loc, mode_t mode,
             mode_t umask, dict_t *xdata)
{
	void *cookie = NULL;
	imess_priv_t *priv = NULL;

	priv = this->private;
	cookie = gf_strdup(loc->path);

	STACK_WIND_COOKIE (frame, imess_mkdir_cbk, cookie,
		           FIRST_CHILD(this), FIRST_CHILD(this)->fops->mkdir,
			   loc, mode, umask, xdata);
	return 0;
}

int
imess_create (call_frame_t *frame, xlator_t *this, loc_t *loc,
              int32_t flags, mode_t mode, mode_t umask, fd_t *fd,
              dict_t *xdata)
{
	void *cookie = NULL;
	imess_priv_t *priv = NULL;

	priv = this->private;
	cookie = gf_strdup(loc->path);

        STACK_WIND_COOKIE (frame, imess_create_cbk, cookie,
			   FIRST_CHILD(this), FIRST_CHILD(this)->fops->create,
			   loc, flags, mode, umask, fd, xdata);
        return 0;
}

int
imess_writev (call_frame_t *frame, xlator_t *this, fd_t *fd,
              struct iovec *vector, int32_t count, off_t offset,
	      uint32_t flags, struct iobref *iobref, dict_t *xdata)
{
	imess_priv_t *priv = NULL;

	priv = this->private;

        STACK_WIND (frame, imess_writev_cbk,
                    FIRST_CHILD(this),
                    FIRST_CHILD(this)->fops->writev,
                    fd, vector, count, offset, flags, iobref, xdata);
	return 0;
}

/*
 * xlator cbks
 */

int32_t
imess_release (xlator_t *this, fd_t *fd)
{
	imess_priv_t *priv = NULL;

	priv = this->private;

        return 0;
}

int32_t
imess_releasedir (xlator_t *this, fd_t *fd)
{
	imess_priv_t *priv = NULL;

	priv = this->private;

        return 0;
}

int32_t
imess_forget (xlator_t *this, inode_t *inode)
{
	imess_priv_t *priv = NULL;

	priv = this->private;

        return 0;
}

/*
 * xlator registration.
 */

int32_t
mem_acct_init (xlator_t *this)
{
	int ret = -1;

	ret = xlator_mem_acct_init (this, gf_imess_mt_end + 1);
	if (ret)
		gf_log (this->name, GF_LOG_ERROR, "Memory accounting "
			"initialization failed.");

	return ret;
}

int32_t
init (xlator_t *this)
{
	int ret = -1;
	dict_t *options = NULL;
	imess_priv_t *priv = NULL;

	GF_VALIDATE_OR_GOTO ("imess", this, out);

        if (!this->children || this->children->next) {
                gf_log (this->name, GF_LOG_ERROR,
                        "FATAL: imess should have exactly one child");
                goto out;
        }

	if (!this->parents) {
		gf_log (this->name, GF_LOG_WARNING,
			"dangling volume. check volfile");
	}

	priv = GF_CALLOC (1, sizeof (*priv), gf_imess_mt_priv_t);
	if (!priv)
		goto out;

	options = this->options;

	GF_OPTION_INIT ("db-path", priv->dbpath, str, out);
	GF_OPTION_INIT ("enable-metacache", priv->enable_metacache, bool, out);

	ret = xdb_init (&priv->xdb, priv->dbpath);
	if (ret) {
		gf_log (this->name, GF_LOG_ERROR,
			"database initialization failed: (%d).", ret);
		goto out;
	}

	gf_log (this->name, GF_LOG_TRACE, "imess initialized. "
		"(database path: %s, metadata cache: %d)",
		priv->dbpath, priv->enable_metacache);

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
	imess_priv_t *priv = NULL;

	priv = this->private;

	GF_FREE (priv);

	return;
}

struct xlator_cbks cbks = {
        .release     = imess_release,
        .releasedir  = imess_releasedir,
        .forget      = imess_forget,
};

struct xlator_fops fops = {
	.lookup      = imess_lookup,
        .stat        = imess_stat,
        .mkdir       = imess_mkdir,
        .create      = imess_create,
        .writev      = imess_writev,
#if 0
        .readlink    = imess_readlink,
        .mknod       = imess_mknod,
        .unlink      = imess_unlink,
        .rmdir       = imess_rmdir,
        .symlink     = imess_symlink,
        .rename      = imess_rename,
        .link        = imess_link,
        .truncate    = imess_truncate,
        .open        = imess_open,
        .readv       = imess_readv,
        .statfs      = imess_statfs,
        .flush       = imess_flush,
        .fsync       = imess_fsync,
        .setxattr    = imess_setxattr,
        .getxattr    = imess_getxattr,
        .fsetxattr   = imess_fsetxattr,
        .fgetxattr   = imess_fgetxattr,
        .removexattr = imess_removexattr,
        .opendir     = imess_opendir,
        .readdir     = imess_readdir,
        .readdirp    = imess_readdirp,
        .fsyncdir    = imess_fsyncdir,
        .access      = imess_access,
        .ftruncate   = imess_ftruncate,
        .fstat       = imess_fstat,
        .lk          = imess_lk,
        .inodelk     = imess_inodelk,
        .finodelk    = imess_finodelk,
        .entrylk     = imess_entrylk,
        .fentrylk    = imess_fentrylk,
        .lookup      = imess_lookup,
        .rchecksum   = imess_rchecksum,
        .xattrop     = imess_xattrop,
        .fxattrop    = imess_fxattrop,
        .setattr     = imess_setattr,
        .fsetattr    = imess_fsetattr,
#endif
};

struct volume_options options [] = {
	{ .key = { "db-path" },
	  .type = GF_OPTION_TYPE_PATH,
	},
	{ .key = { "enable-metacache" },
	  .type = GF_OPTION_TYPE_BOOL,
	},
	{ .key = { NULL } },
};

