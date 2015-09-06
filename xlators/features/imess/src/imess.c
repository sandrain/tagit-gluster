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

	if (priv->commit_mode == IMESS_COMMIT_PARANOID)
		xdb_tx_begin(xdb);

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
	if (ret) {
		gf_log("imess", GF_LOG_ERROR,
		       "put_stat_attr: xdb_insert_stat failed (%s)",
		       xdb->err);
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_abort(xdb);
	}
	else {
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_commit(xdb);
	}

out:
	return ret;
}

/*
 * xlator callbacks.
 */

#if 0
int
imess_stat_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		int32_t op_ret, int32_t op_errno, struct iatt *buf,
		dict_t *xdata)
{
	IMESS_STACK_UNWIND(stat, frame, op_ret, op_errno, buf, xdata);
	return 0;
}
#endif

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

	if (op_ret == -1)
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
imess_unlink_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                  int32_t op_ret, int32_t op_errno,
                  struct iatt *preparent, struct iatt *postparent,
                  dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;
	xdb_t *xdb = NULL;
	xdb_file_t file = { 0, };

	priv = this->private;
	xdb = priv->xdb;
	file.gfid = cookie;

	if (op_ret == -1)
		goto out;

	if (priv->commit_mode == IMESS_COMMIT_PARANOID)
		xdb_tx_begin(xdb);

	ret = xdb_remove_file(xdb, &file);
	if (ret) {
		gf_log(this->name, GF_LOG_WARNING, "imess_unlink_cbk: "
				"xdb_remove_file failed");
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_abort(xdb);
	}
	else {
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_commit(xdb);
	}

out:
        IMESS_STACK_UNWIND (unlink, frame, op_ret, op_errno,
                            preparent, postparent, xdata);
	return 0;
}

int
imess_rmdir_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int32_t op_ret, int32_t op_errno,
                 struct iatt *preparent, struct iatt *postparent,
		 dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;
	xdb_t *xdb = NULL;
	xdb_file_t file = { 0, };

	priv = this->private;
	xdb = priv->xdb;
	file.gfid = cookie;

	if (op_ret == -1)
		goto out;

	if (priv->commit_mode == IMESS_COMMIT_PARANOID)
		xdb_tx_begin(xdb);

	ret = xdb_remove_file(priv->xdb, &file);
	if (ret) {
		gf_log(this->name, GF_LOG_WARNING, "imess_rmdir_cbk: "
				"xdb_remove_file failed");
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_abort(xdb);
	}
	else {
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_commit(xdb);
	}

out:
        IMESS_STACK_UNWIND (rmdir, frame, op_ret, op_errno,
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

	if (op_ret == -1)
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
imess_symlink_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                   int32_t op_ret, int32_t op_errno,
                   inode_t *inode, struct iatt *buf,
                   struct iatt *preparent, struct iatt *postparent,
                   dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;

	priv = this->private;

	if (op_ret == -1)
		goto out;

	ret = put_stat_attr(priv, buf, (const char *) cookie);
	if (ret)
		gf_log(this->name, GF_LOG_ERROR, "imess_symlink_cbk: "
				"xdb_insert_stat failed");

out:
	GF_FREE(cookie);
        IMESS_STACK_UNWIND (symlink, frame, op_ret, op_errno, inode, buf,
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
	xdb_t *xdb = NULL;
	xdb_file_t file = {0,};
	struct stat sb = {0,};

	priv = this->private;
	xdb = priv->xdb;

	if (op_ret == -1)
		goto out;

	iatt_to_stat(postbuf, &sb);
	file.gfid = (const char *) cookie;

	if (prebuf->ia_mtime == postbuf->ia_mtime
	    && prebuf->ia_size == postbuf->ia_size
	    && prebuf->ia_blocks == postbuf->ia_blocks)
		goto pass;

	if (priv->commit_mode == IMESS_COMMIT_PARANOID)
		xdb_tx_begin(xdb);

	/* mtime */
	if (prebuf->ia_mtime != postbuf->ia_mtime) {
		ret = xdb_update_stat(xdb, &file, &sb, XDB_ST_MTIME);
		if (ret)
			goto out;
	}

	/* size */
	if (prebuf->ia_size != postbuf->ia_size) {
		ret = xdb_update_stat(xdb, &file, &sb, XDB_ST_SIZE);
		if (ret)
			goto out;
	}

	/* blocks */
	if (prebuf->ia_blocks != postbuf->ia_blocks) {
		ret = xdb_update_stat(xdb, &file, &sb, XDB_ST_BLOCKS);
		if (ret)
			goto out;
	}

out:
	if (ret) {
		gf_log(this->name, GF_LOG_WARNING,
			"imess_writev_cbk: %s", xdb->err);
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_abort(xdb);
	}
	else {
		if (priv->commit_mode == IMESS_COMMIT_PARANOID)
			xdb_tx_commit(xdb);
	}
pass:
	FREE (cookie);
        IMESS_STACK_UNWIND (writev, frame, op_ret, op_errno, prebuf, postbuf,
                            xdata);
        return 0;
}

#if 0
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
#endif

int
imess_fsync_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                 int32_t op_ret, int32_t op_errno,
                 struct iatt *prebuf, struct iatt *postbuf, dict_t *xdata)
{
	int ret = 0;
	imess_priv_t *priv = NULL;
	xdb_t *xdb = NULL;
	int datasync = 0;

	priv = this->private;
	xdb = priv->xdb;
	datasync = *((int32_t *) cookie);

	if (datasync)
		goto out;

	if (priv->commit_mode == IMESS_COMMIT_SYNC) {
		xdb_tx_commit(priv->xdb);
		xdb_tx_begin(priv->xdb);
	}

out:
	FREE(cookie);
        IMESS_STACK_UNWIND (fsync, frame, op_ret, op_errno, prebuf, postbuf,
                            xdata);
        return ret;
}

#if 0
int
imess_stat (call_frame_t *frame, xlator_t *this, loc_t *loc, dict_t *xdata)
{
	imess_priv_t *priv = NULL;

	priv = this->private;

	if (!priv->lookup_cache)
		goto call_child;

	/* TODO: implement the cache logic (key: loc->inode->gfid) */

call_child:
	STACK_WIND (frame, imess_stat_cbk, FIRST_CHILD(this),
		    FIRST_CHILD(this)->fops->stat,
		    loc, xdata);
	return 0;
}
#endif

int
imess_mkdir (call_frame_t *frame, xlator_t *this, loc_t *loc, mode_t mode,
             mode_t umask, dict_t *xdata)
{
	void *cookie = NULL;

	cookie = gf_strdup(loc->path);

	STACK_WIND_COOKIE (frame, imess_mkdir_cbk, cookie,
		           FIRST_CHILD(this), FIRST_CHILD(this)->fops->mkdir,
			   loc, mode, umask, xdata);
	return 0;
}

int
imess_unlink (call_frame_t *frame, xlator_t *this, loc_t *loc, int xflag,
              dict_t *xdata)
{
	void *cookie = NULL;

	cookie = uuid_utoa(loc->inode->gfid);

        STACK_WIND_COOKIE (frame, imess_unlink_cbk, cookie,
                           FIRST_CHILD(this),
                           FIRST_CHILD(this)->fops->unlink,
                           loc, xflag, xdata);
        return 0;
}

int
imess_rmdir (call_frame_t *frame, xlator_t *this, loc_t *loc, int flags,
             dict_t *xdata)
{
	void *cookie = NULL;

	cookie = uuid_utoa(loc->inode->gfid);

        STACK_WIND_COOKIE (frame, imess_rmdir_cbk, cookie,
                           FIRST_CHILD(this),
                           FIRST_CHILD(this)->fops->rmdir,
                           loc, flags, xdata);
        return 0;
}

int
imess_create (call_frame_t *frame, xlator_t *this, loc_t *loc,
              int32_t flags, mode_t mode, mode_t umask, fd_t *fd,
              dict_t *xdata)
{
	void *cookie = NULL;

	cookie = gf_strdup(loc->path);

        STACK_WIND_COOKIE (frame, imess_create_cbk, cookie,
			   FIRST_CHILD(this), FIRST_CHILD(this)->fops->create,
			   loc, flags, mode, umask, fd, xdata);
        return 0;
}

int
imess_symlink (call_frame_t *frame, xlator_t *this, const char *linkpath,
               loc_t *loc, mode_t umask, dict_t *xdata)
{
	void *cookie = NULL;

	cookie = gf_strdup(loc->path);

        STACK_WIND_COOKIE (frame, imess_symlink_cbk, cookie,
			   FIRST_CHILD(this), FIRST_CHILD(this)->fops->symlink,
			   linkpath, loc, umask, xdata);
        return 0;
}


int
imess_writev (call_frame_t *frame, xlator_t *this, fd_t *fd,
              struct iovec *vector, int32_t count, off_t offset,
	      uint32_t flags, struct iobref *iobref, dict_t *xdata)
{
	void *cookie = NULL;

	cookie = uuid_utoa(fd->inode->gfid);

        STACK_WIND_COOKIE (frame, imess_writev_cbk, cookie,
                           FIRST_CHILD(this),
                           FIRST_CHILD(this)->fops->writev,
                           fd, vector, count, offset, flags, iobref, xdata);
	return 0;
}

int
imess_fsync (call_frame_t *frame, xlator_t *this, fd_t *fd, int32_t datasync,
             dict_t *xdata)
{
	int *cookie = MALLOC(sizeof(datasync));

	*cookie = datasync;

        STACK_WIND_COOKIE (frame, imess_fsync_cbk, (void *) cookie,
                           FIRST_CHILD(this),
                           FIRST_CHILD(this)->fops->fsync,
                           fd, datasync, xdata);
	return 0;
}

int
imess_ipc (call_frame_t *frame, xlator_t *this, int op, dict_t *xdata)
{
	int op_ret = -1;
	int op_errno = 0;
	dict_t *xdout = NULL;
	imess_priv_t *priv = NULL;
	xdb_t *xdb = NULL;
	char *req = NULL;

	priv = this->private;
	xdb = priv->xdb;

	/** TODO: shit, fix this */
	op_ret = dict_get_str (xdata, "sql", &req);
	if (op_ret == 0) {
		xdout = dict_new ();

		op_ret = xdb_direct_query (xdb, req, xdout);
		if (op_ret) {
			op_errno = EIO;
			goto out;
		}
	}
	else {
		op_ret = dict_get_str (xdata, "query", &req);
		xdout = dict_new ();

		if (!strcmp(req, "xfile")) {
			op_ret = xdb_read_all_xfile (xdb, xdout);
			if (op_ret) {
				op_errno = EIO;
				goto out;
			}
		}
		else if (!strcmp(req, "xname")) {
			op_ret = xdb_read_all_xname (xdb, xdout);
			if (op_ret) {
				op_errno = EIO;
				goto out;
			}
		}
		else if (!strcmp(req, "xdata")) {
			op_ret = xdb_read_all_xdata (xdb, xdout);
			if (op_ret) {
				op_errno = EIO;
				goto out;
			}
		}
		else {
			/* do error handling */
			op_errno = EINVAL;
			goto out;
		}
	}

	op_ret = 0;

out:
	STACK_UNWIND_STRICT (ipc, frame, op_ret, op_errno, xdout);

	if (xdout)
		dict_unref (xdout);

	return 0;
}

/*
 * xlator cbks
 */

int32_t
imess_release (xlator_t *this, fd_t *fd)
{
        return 0;
}

int32_t
imess_releasedir (xlator_t *this, fd_t *fd)
{
        return 0;
}

int32_t
imess_forget (xlator_t *this, inode_t *inode)
{
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
	char *commit_mode = NULL;
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
	GF_OPTION_INIT ("commit-mode", commit_mode, str, out);
	GF_OPTION_INIT ("enable-lookup-cache", priv->lookup_cache, bool, out);

	if (0 == strcmp(commit_mode, "lazy"))
		priv->commit_mode = IMESS_COMMIT_LAZY;
	else if (0 == strcmp(commit_mode, "paranoid"))
		priv->commit_mode = IMESS_COMMIT_PARANOID;
	else if (0 == strcmp(commit_mode, "dynamic"))
		priv->commit_mode = IMESS_COMMIT_DYNAMIC;
	else	/* default (including non-senses) falls back to the sync mode */
		priv->commit_mode = IMESS_COMMIT_SYNC;

	ret = xdb_init (&priv->xdb, priv->dbpath);
	if (ret) {
		gf_log (this->name, GF_LOG_ERROR,
			"database initialization failed: (%d).", ret);
		goto out;
	}

	if (priv->commit_mode != IMESS_COMMIT_PARANOID)
		xdb_tx_begin (priv->xdb);

	gf_log (this->name, GF_LOG_INFO, "imess initialized. "
		"(database path: %s, lookup cache: %d, commit mode: %s)",
		priv->dbpath, priv->lookup_cache, commit_mode);

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

	if (priv->commit_mode == IMESS_COMMIT_PARANOID)
		xdb_tx_commit (priv->xdb);

	xdb_exit (priv->xdb);

	GF_FREE (priv);

	return;
}

struct xlator_cbks cbks = {
        .release     = imess_release,
        .releasedir  = imess_releasedir,
        .forget      = imess_forget,
};

struct xlator_fops fops = {
        .mkdir       = imess_mkdir,
        .unlink      = imess_unlink,
        .rmdir       = imess_rmdir,
        .create      = imess_create,
        .symlink     = imess_symlink,
        .writev      = imess_writev,
        .fsync       = imess_fsync,
	.ipc         = imess_ipc,
#if 0
        .stat        = imess_stat,
	.lookup      = imess_lookup,
        .fsyncdir    = imess_fsyncdir,
        .readlink    = imess_readlink,
        .mknod       = imess_mknod,
        .unlink      = imess_unlink,
        .rmdir       = imess_rmdir,
        .rename      = imess_rename,
        .link        = imess_link,
        .truncate    = imess_truncate,
        .open        = imess_open,
        .readv       = imess_readv,
        .statfs      = imess_statfs,
        .setxattr    = imess_setxattr,
        .getxattr    = imess_getxattr,
        .fsetxattr   = imess_fsetxattr,
        .fgetxattr   = imess_fgetxattr,
        .removexattr = imess_removexattr,
        .opendir     = imess_opendir,
        .readdir     = imess_readdir,
        .readdirp    = imess_readdirp,
        .access      = imess_access,
        .ftruncate   = imess_ftruncate,
        .fstat       = imess_fstat,
        .lk          = imess_lk,
        .inodelk     = imess_inodelk,
        .finodelk    = imess_finodelk,
        .entrylk     = imess_entrylk,
        .fentrylk    = imess_fentrylk,
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
	{ .key = { "enable-lookup-cache" },
	  .type = GF_OPTION_TYPE_BOOL,
	},
	{ .key = { "commit-mode" },
          .type = GF_OPTION_TYPE_STR,
	},
	{ .key = { NULL } },
};

