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
 * helper functions
 */

static int
insert_new_xdb_entry (ims_priv_t *priv, struct iatt *buf, const char *path)
{
	int ret              = 0;

	if (priv->async_update) {
		ims_task_t task = { {0,0}, };

		task.op = IMS_TASK_INSERT_NEW_FILE;

		/* this should be GF_FREE-ed by the async worker function */
		task.file.gfid = uuid_utoa (buf->ia_gfid);
		task.file.path = path;

		iatt_to_stat (buf, &task.sb);

		ret = ims_async_put_task (priv->async_ctx, &task);
	}
	else {
		ims_xdb_file_t file = { 0, };
		struct stat sb      = { 0, };
		ims_xdb_t *xdb      = NULL;

		xdb = priv->xdb;

		file.gfid = uuid_utoa (buf->ia_gfid);
		file.path = path;

		iatt_to_stat (buf, &sb);

		ret = ims_xdb_insert_new_file (xdb, &file, &sb);
	}

	return ret;
}

static inline int
unlink_from_xdb (ims_priv_t *priv, const char *path)
{
	int ret             = 0;

	if (priv->async_update) {
		ims_task_t task = { {0,0}, };

		task.op = IMS_TASK_UNLINK_FILE;
		task.file.path = path;

		ret = ims_async_put_task (priv->async_ctx, &task);
	}
	else {
		ims_xdb_file_t file = { 0, };
		ims_xdb_t *xdb      = NULL;

		xdb = priv->xdb;

		file.path = path;

		ret = ims_xdb_unlink_file (xdb, &file);
	}

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
	const char *path = NULL;
	ims_priv_t *priv = NULL;

	if (op_ret == -1)
		goto out;

	/* don't populate db if non-hashed location. */
	path = (const char *) cookie;
	if (path == NULL)
		goto out;

	priv = this->private;
	ret = insert_new_xdb_entry (priv, buf, (const char *) cookie);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_mkdir_cbk: populate_xdb failed "
			"(ret=%d, db_ret=%d)",
			ret, priv->xdb->db_ret);

out:
	STACK_UNWIND_STRICT (mkdir, frame, op_ret, op_errno, inode, buf,
			     preparent, postparent, xdata);
	return 0;
}

int32_t
ims_mkdir (call_frame_t *frame, xlator_t *this, loc_t *loc, mode_t mode,
	   mode_t umask, dict_t *xdata)
{
	int         ret     = 0;
	int8_t      hashed  = 0;
	ims_priv_t *priv    = NULL;
	const char *path    = NULL;

	priv = this->private;
	path = loc->path;

	if (priv->dir_hash) {
		ret = dict_get_int8 (xdata, "imess-dht-hashed", &hashed);
		if (ret) {
			gf_log (this->name, GF_LOG_WARNING,
				"ims_mkdir: imess-dht-hashed is not set.");
			goto wind;
		}

		/* don't populate if non-hashed location */
		if (0 == hashed)
			path = NULL;
	}

wind:
	STACK_WIND_COOKIE (frame, ims_mkdir_cbk, (void *) path,
			   FIRST_CHILD (this),
			   FIRST_CHILD (this)->fops->mkdir,
			   loc, mode, umask, xdata);
	return 0;
}

/*
 * unlink
 */
int32_t
ims_unlink_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		int32_t op_ret, int32_t op_errno,
		struct iatt *preparent, struct iatt *postparent,
		dict_t *xdata)
{
	int ret             = 0;
	ims_priv_t *priv    = NULL;

	if (op_ret == -1)
		goto out;

	priv = this->private;

	ret = unlink_from_xdb (priv, cookie);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_unlink_cbk: xdb_remove_file failed "
			"(ret=%d, db_ret=%d)",
			ret, priv->xdb->db_ret);

out:
	STACK_UNWIND_STRICT (unlink, frame, op_ret, op_errno,
			     preparent, postparent, xdata);
	return 0;
}

int32_t
ims_unlink (call_frame_t *frame, xlator_t *this, loc_t *loc, int xflag,
	    dict_t *xdata)
{
	STACK_WIND_COOKIE (frame, ims_unlink_cbk, (void *) loc->path,
			   FIRST_CHILD (this),
			   FIRST_CHILD (this)->fops->unlink,
			   loc, xflag, xdata);
	return 0;
}

/*
 * rmdir
 *
 * for directories, no hardlinks.
 */

int32_t
ims_rmdir_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
	       int32_t op_ret, int32_t op_errno,
	       struct iatt *preparent, struct iatt *postparent,
	       dict_t *xdata)
{
	int ret             = 0;
	ims_priv_t *priv    = NULL;

	if (op_ret == -1)
		goto out;

	priv = this->private;
	ret = unlink_from_xdb (priv, cookie);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_rmdir_cbk: xdb_remove_file failed "
			"(ret=%d, db_ret=%d)",
			ret, priv->xdb->db_ret);

out:
	STACK_UNWIND_STRICT (rmdir, frame, op_ret, op_errno,
			     preparent, postparent, xdata);
	return 0;
}

int32_t
ims_rmdir (call_frame_t *frame, xlator_t *this, loc_t *loc, int flags,
	   dict_t *xdata)
{
	STACK_WIND_COOKIE (frame, ims_rmdir_cbk, (void *) loc->path,
			   FIRST_CHILD(this),
			   FIRST_CHILD(this)->fops->rmdir,
			   loc, flags, xdata);
	return 0;
}

/*
 * setxattr
 */

#if 0
struct _ims_setxattr_handler_data {
	ims_xdb_t      *xdb;
	ims_xdb_file_t *file;
};

typedef struct _ims_xattr_handler_data ims_setxattr_handler_data_t;

static inline int
parse_value_type (data_t *v)
{
	int i        = 0;
	int32_t len  = 0;
	char *data   = NULL;
	int type     = IMS_XDB_TYPE_NONE;

	data = (char *) data_to_ptr (v);
	len = v->len;

	if (data[0] != '-' || isdigit (data[0]))
		goto try_string;

	for (i = 1; i < len; i++) {
		if (!isdigit (data[i]))
			goto try_string;
	}

	type = IMS_XDB_TYPE_INTEGER;
	goto out;

try_string:
	for (i = 0; i < len; i++) {
		if (!isprint (data[i]))
			goto out;
	}

	type = IMS_XDB_TYPE_STRING;

out:
	return type;
}

static int
handle_setxattr_kv (dict_t *dict, char *k, data_t *v, void *tmp)
{
	int ret                           = 0;
	int type                          = 0;
	ims_xdb_t *xdb                    = NULL;
	ims_xdb_file_t *file              = NULL;
	ims_xdb_attr_t xattr              = { 0, };
	ims_setxattr_handler_data_t *data = NULL;

	if (XATTR_IS_PATHINFO (k))
		goto out;
	else if (ZR_FILE_CONTENT_REQUEST (k))
		goto out;
	else if (GF_POSIX_ACL_REQUEST (k))
		goto out;

	data = (ims_setxattr_handler_data_t *) tmp;

	type = parse_value_type (v);
	if (type == IMS_XDB_TYPE_NONE)
		goto out;

	xdb = data->xdb;
	file = data->file;

	xattr.name = k;
	xattr.type = type;

	if (type == IMS_XDB_TYPE_INTEGER)
		xattr.ival = data_to_int64 (v);
	else
		xattr.sval = data_to_str (v);

	ret = ims_xdb_insert_xattr (xdb, file, &xattr, 1);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
				"ims_setxattr_cbk: ims_xdb_insert_xattr failed "
				"(ret=%d, db_ret=%d)",
				ret, priv->xdb->db_ret);

out:
	return 0;
}

int32_t
ims_setxattr_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		  int32_t op_ret, int32_t op_errno, dict_t *xdata)
{
	int ret                                 = 0;
	ims_priv_t *priv                        = NULL;
	ims_xdb_file_t file                     = { 0, };
	ims_setxattr_handler_data_t filler_data = { 0, };

	if (op_ret == -1)
		goto out;

	priv = this->private;
	file.gfid = cookie;

	filler_data->xdb  = priv->xdb;
	filler_data->file = &file;

	ret = dict_foreach (xdata, handle_setxattr_kv, &filler_data);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
				"ims_setxattr_cbk: handle_setxattr_kv failed "
				"(ret=%d, db_ret=%d)",
				ret, priv->xdb->db_ret);

out:
	STACK_UNWIND_STRICT (setxattr, frame, op_ret, op_errno, xdata);
	return 0;
}


int32_t
ims_setxattr (call_frame_t *frame, xlator_t *this,
	      loc_t *loc, dict_t *dict, int flags, dict_t *xdata)
{
	void *cookie = NULL;

	cookie = uuid_utoa (loc->inode->gfid);

	STACK_WIND_COOKIE (frame, ims_setxattr_cbk, cookie,
			FIRST_CHILD (this),
			FIRST_CHILD (this)->fops->setxattr,
			loc, dict, flags, xdata);
	return 0;
}
#endif

/*
 * ftruncate
 */
int
ims_setattr_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		 int32_t op_ret, int32_t op_errno,
		 struct iatt *preop, struct iatt *postop, dict_t *xdata);

int32_t
ims_ftruncate (call_frame_t *frame, xlator_t *this,
	       fd_t *fd, off_t offset, dict_t *xdata)
{
	STACK_WIND (frame, ims_setattr_cbk,
		    FIRST_CHILD(this),
		    FIRST_CHILD(this)->fops->ftruncate,
		    fd, offset, xdata);
	return 0;
}

/*
 * create
 */

int32_t
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

	ret = insert_new_xdb_entry (priv, buf, (const char *) cookie);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_create_cbk: populate_xdb failed "
			"(ret=%d, db_ret=%d)",
			ret, priv->xdb->db_ret);

out:
	STACK_UNWIND_STRICT (create, frame, op_ret, op_errno, fd, inode, buf,
			preparent, postparent, xdata);
	return 0;
}

int32_t
ims_create (call_frame_t *frame, xlator_t *this, loc_t *loc, int32_t flags,
	    mode_t mode, mode_t umask, fd_t *fd, dict_t *xdata)
{
	STACK_WIND_COOKIE (frame, ims_create_cbk, (void *) loc->path,
			   FIRST_CHILD (this),
			   FIRST_CHILD( this)->fops->create,
			   loc, flags, mode, umask, fd, xdata);
	return 0;
}

/*
 * setattr
 */

int
ims_setattr_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		 int32_t op_ret, int32_t op_errno,
		 struct iatt *preop, struct iatt *postop, dict_t *xdata)
{
	int ret             = 0;
	ims_priv_t *priv    = NULL;
	ims_xdb_t *xdb      = NULL;

	if (op_ret == -1)
		goto out;

	if (gf_uuid_is_null (postop->ia_gfid))
		goto out;

	priv = this->private;

	if (priv->async_update) {
		ims_task_t task = { {0,0}, };

		iatt_to_stat (postop, &task.sb);
		task.op = IMS_TASK_UPDATE_STAT;
		task.file.gfid = uuid_utoa (postop->ia_gfid);

		ret = ims_async_put_task (priv->async_ctx, &task);
	}
	else {
		struct stat sb      = { 0, };
		ims_xdb_file_t file = { 0, };

		xdb = priv->xdb;
		iatt_to_stat (postop, &sb);

		file.gfid = uuid_utoa (postop->ia_gfid);

		ret = ims_xdb_update_stat (xdb, &file, &sb,
					   IMS_XDB_STAT_OP_WRITE);
	}

	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_setattr_cbk: ims_xdb_update_stat failed "
			"(ret=%d, db_ret=%d)",
			ret, xdb->db_ret);

out:
	STACK_UNWIND_STRICT (setattr, frame, op_ret, op_errno,
			preop, postop, xdata);
	return 0;
}

int32_t
ims_setattr (call_frame_t *frame, xlator_t *this,
	     loc_t *loc, struct iatt *stbuf, int32_t valid, dict_t *xdata)
{
	STACK_WIND (frame, ims_setattr_cbk,
		    FIRST_CHILD(this),
		    FIRST_CHILD(this)->fops->setattr,
		    loc, stbuf, valid, xdata);
	return 0;
}

/*
 * fsetattr
 */

int32_t
ims_fsetattr (call_frame_t *frame, xlator_t *this,
	      fd_t *fd, struct iatt *stbuf, int32_t valid, dict_t *xdata)
{
	STACK_WIND (frame, ims_setattr_cbk,
		    FIRST_CHILD (this),
		    FIRST_CHILD (this)->fops->fsetattr,
		    fd, stbuf, valid, xdata);
	return 0;
}

/*
 * symlink
 */

int32_t
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

	ret = insert_new_xdb_entry (priv, buf, (const char *) cookie);
	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_symlink_cbk: populate_xdb failed "
			"(ret=%d, db_ret=%d)",
			ret, priv->xdb->db_ret);

out:
	STACK_UNWIND_STRICT (symlink, frame, op_ret, op_errno, inode, buf,
			     preparent, postparent, xdata);
	return 0;
}


int32_t
ims_symlink (call_frame_t *frame, xlator_t *this, const char *linkpath,
	     loc_t *loc, mode_t umask, dict_t *xdata)
{
	STACK_WIND_COOKIE (frame, ims_symlink_cbk, (void *) loc->path,
			   FIRST_CHILD (this),
			   FIRST_CHILD (this)->fops->symlink,
			   linkpath, loc, umask, xdata);
	return 0;
}

/*
 * link
 */

int32_t
ims_link_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
	      int32_t op_ret, int32_t op_errno,
	      inode_t *inode, struct iatt *buf,
	      struct iatt *preparent, struct iatt *postparent,
	      dict_t *xdata)
{
	int ret             = 0;
	ims_priv_t *priv    = NULL;
	ims_xdb_t *xdb      = NULL;

	if (op_ret == -1)
		goto out;

	priv = this->private;

	if (priv->async_update) {
		ims_task_t task = { {0,0}, };

		task.op = IMS_TASK_LINK_FILE;
		task.file.path = (char *) cookie;
		task.file.gfid = uuid_utoa (buf->ia_gfid);

		ret = ims_async_put_task (priv->async_ctx, &task);
	}
	else {
		ims_xdb_file_t file = { 0, };

		xdb = priv->xdb;

		file.path = (char *) cookie;
		file.gfid = uuid_utoa (buf->ia_gfid);

		ret = ims_xdb_link_file (xdb, &file);
	}

	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_link_cbk: ims_xdb_link_file failed "
			"(ret=%d, db_ret=%d)",
			ret, xdb->db_ret);

out:
	STACK_UNWIND_STRICT (link, frame, op_ret, op_errno, inode, buf,
			     preparent, postparent, xdata);
	return 0;
}

int32_t
ims_link (call_frame_t *frame, xlator_t *this,
	  loc_t *oldloc, loc_t *newloc, dict_t *xdata)
{
	STACK_WIND_COOKIE (frame, ims_link_cbk, (void *) newloc->path,
			   FIRST_CHILD (this),
			   FIRST_CHILD (this)->fops->link,
			   oldloc, newloc, xdata);
	return 0;
}

/*
 * truncate
 */

int32_t
ims_truncate (call_frame_t *frame, xlator_t *this,
	      loc_t *loc, off_t offset, dict_t *xdata)
{
	STACK_WIND (frame, ims_setattr_cbk,
		    FIRST_CHILD(this),
		    FIRST_CHILD(this)->fops->truncate,
		    loc, offset, xdata);
	return 0;
}

/*
 * writev
 */

int32_t
ims_writev (call_frame_t *frame, xlator_t *this,
	    fd_t *fd, struct iovec *vector,
	    int32_t count, off_t offset,
	    uint32_t flags, struct iobref *iobref, dict_t *xdata)
{
	STACK_WIND (frame, ims_setattr_cbk,
		    FIRST_CHILD(this),
		    FIRST_CHILD(this)->fops->writev,
		    fd, vector, count, offset, flags, iobref, xdata);
	return 0;
}

/*
 * rename
 */

struct _ims_rename_data {
	char *old_path;
	char *new_path;
};

typedef struct _ims_rename_data ims_rename_data_t;

int32_t
ims_rename_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
		int32_t op_ret, int32_t op_errno, struct iatt *buf,
		struct iatt *preoldparent, struct iatt *postoldparent,
		struct iatt *prenewparent, struct iatt *postnewparent,
		dict_t *xdata)
{
	int ret                        = 0;
	ims_priv_t *priv               = NULL;
	ims_xdb_t *xdb                 = NULL;
	ims_rename_data_t *rename_data = NULL;

	if (op_ret == -1)
		goto out;

	if (cookie == NULL)
		goto out;

	priv = this->private;
	rename_data = cookie;

	if (priv->async_update) {
		ims_task_t task = { {0,0}, };

		task.op = IMS_TASK_RENAME;
		task.file.path = rename_data->old_path;
		task.file.extra = (void *) rename_data->new_path;

		ret = ims_async_put_task (priv->async_ctx, &task);
	}
	else {
		ims_xdb_file_t file            = { 0, };

		xdb = priv->xdb;

		file.path = rename_data->old_path;
		file.extra = (void *) rename_data->new_path;

		ret = ims_xdb_rename (xdb, &file);
	}

	if (ret)
		gf_log (this->name, GF_LOG_WARNING,
			"ims_rename_cbk: ims_xdb_rename failed "
			"(ret=%d, db_ret=%d))",
			ret, xdb->db_ret);

out:
	FREE (cookie);
	STACK_UNWIND_STRICT (rename, frame, op_ret, op_errno, buf,
			     preoldparent, postoldparent,
			     prenewparent, postnewparent, xdata);
	return 0;
}

int32_t
ims_rename (call_frame_t *frame, xlator_t *this,
	    loc_t *oldloc, loc_t *newloc, dict_t *xdata)
{
	ims_rename_data_t *rename_data = NULL;

	rename_data = CALLOC (1, sizeof(*rename_data));
	if (!rename_data) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_rename: CALLOC failed");
		goto out;
	}

	rename_data->old_path = (char *) oldloc->path;
	rename_data->new_path = (char *) newloc->path;
out:
	STACK_WIND_COOKIE (frame, ims_rename_cbk, (void *) rename_data,
			   FIRST_CHILD (this),
			   FIRST_CHILD (this)->fops->rename,
			   oldloc, newloc, xdata);
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
	dict_t *xdout    = NULL;
	char *type       = NULL;

	if (op != IMESS_IPC_OP) {
		STACK_WIND (frame, ims_ipc_cbk,
			    FIRST_CHILD (this), FIRST_CHILD (this)->fops->ipc,
			    op, xdata);
		return 0;
	}

	/* read the request type */
	op_ret = dict_get_str (xdata, "type", &type);
	if (op_ret) {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: IPC request without a type specified.");
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

	if (strncmp (type, "query", strlen("query")) == 0) {
		op_ret = ims_ipc_query (this, xdata, xdout, &op_errno);
		if (op_ret)
			goto out;
	}
	else if (strncmp (type, "filter", strlen("filter")) == 0) {
		op_ret = ims_ipc_filter (this, xdata, xdout, &op_errno);
		if (op_ret)
			goto out;
	}
	else if (strncmp (type, "extractor", strlen("extractor")) == 0) {
		op_ret = ims_ipc_extractor (this, xdata, xdout, &op_errno);
		if (op_ret)
			goto out;
	}
	else {
		gf_log (this->name, GF_LOG_WARNING,
			"ims_ipc: unknown IPC type %s.", type);
		op_errno = EINVAL;
	}

out:
	if (type)
		GF_FREE (type);

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
	int mode	   = 0;
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
	GF_OPTION_INIT ("enable-async-update", priv->async_update, bool, out);
	GF_OPTION_INIT ("enable-dir-hash", priv->dir_hash, bool, out);

	if (priv->async_update)
		mode = XDB_MODE_ASYNC;

	ret = ims_xdb_init (&priv->xdb, priv->db_path, mode);
	if (ret) {
		gf_log (this->name, GF_LOG_ERROR,
			"FATAL: ims_xdb_init failed (ret=%d)", ret);
		goto out;
	}

	if (!priv->async_update)
		goto success;

	ret = ims_async_init (&priv->async_ctx, priv->xdb);
	if (ret) {
		gf_log (this->name, GF_LOG_ERROR,
			"FATAL: ims_async_init failed (ret=%d)", ret);
		goto out;
	}

	/* we need another db connection */
	ret = ims_xdb_init (&priv->xdb, priv->db_path, mode);
	if (ret) {
		gf_log (this->name, GF_LOG_ERROR,
			"FATAL: ims_xdb_init (2nd) failed (ret=%d)", ret);
		goto out;
	}

success:
	gf_log (this->name, GF_LOG_INFO,
		"imess-server initialized: db-path=%s, lookup-cache=%d, "
		"async-update=%d, dir-hash=%d",
		priv->db_path, priv->lookup_cache,
		priv->async_update, priv->dir_hash);
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
	.unlink       = ims_unlink,
	.rmdir        = ims_rmdir,
	.symlink      = ims_symlink,
	.rename       = ims_rename,
	.link         = ims_link,
	.truncate     = ims_truncate,
	.writev       = ims_writev,
#if 0
	.setxattr     = ims_setxattr,
	.getxattr     = ims_getxattr,
	.removexattr  = ims_removexattr,
	.fsetxattr    = ims_fsetxattr,
	.fgetxattr    = ims_fgetxattr,
	.fremovexattr = ims_fremovexattr,
#endif
	.ftruncate    = ims_ftruncate,
	.create       = ims_create,
	.setattr      = ims_setattr,
	.fsetattr     = ims_fsetattr,
#if 0
	.fallocate    = ims_fallocate,
	.discard      = ims_discard,
	.zerofill     = ims_zerofill,
#endif
	.ipc          = ims_ipc,
};

struct volume_options options [] = {
	{ .key = { "db-path" },
	  .type = GF_OPTION_TYPE_PATH,
	  .description = "Index database file path."
	},
	{ .key = { "enable-lookup-cache" },
	  .type = GF_OPTION_TYPE_BOOL,
	  .default_value = "off",
	  .description = "Turn on the lookup cache to speed up.",
	},
	{ .key = { "enable-async-update" },
	  .type = GF_OPTION_TYPE_BOOL,
	  .default_value = "off",
	  .description = "Turn on the asynchronous database update "
			 "to speed up.",
	},
	{ .key = { "enable-dir-hash" },
	  .type = GF_OPTION_TYPE_BOOL,
	  .default_value = "off",
	  .description = "Turn on the dir hashing to store the directory "
			 "index record on a single brick",
	},
	{ .key = { NULL } },
};
