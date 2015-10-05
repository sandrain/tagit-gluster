#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "xlator.h"
#include "defaults.h"
#include "syncop.h"

#include "meta-mem-types.h"
#include "meta.h"
#include "meta-hooks.h"

/*
 * helper functions
 */
static inline char *get_client_name(inode_t *inode)
{
	inode_t *parent_inode = NULL;
	dentry_t *dentry = NULL;

	dentry = list_first_entry (&inode->dentry_list, dentry_t, inode_list);

	parent_inode = dentry->parent;
	dentry = list_first_entry (&parent_inode->dentry_list, dentry_t,
					inode_list);

	return dentry->name;
}

#define _llu(x)	((unsigned long long) (x))

static inline int file_fill_all_records(strfd_t *strfd, dict_t *xdata)
{
	int ret = 0;
	uint64_t i = 0;
	uint64_t count = 0;
	char keybuf[8] = { 0, };
	char *row = NULL;

	ret = dict_get_uint64 (xdata, "count", &count);
	if (ret)
		return -1;

	for (i = 0; i < count; i++) {
		sprintf(keybuf, "%llu", _llu(i));
		ret = dict_get_str (xdata, keybuf, &row);

		strprintf(strfd, "[%7llu] %s\n", _llu(i+1), row);
	}

	strprintf(strfd, "\n%llu records\n", _llu(count));

	return strfd->size;
}

/*
 * xfile
 */

static int
imess_xfile_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	int ret = 0;
	int op = 0;
	xlator_t *xl = NULL;
	char *client = NULL;
	dict_t *xdin = NULL;
	dict_t *xdout = NULL;

	xl = this->children->xlator;
	client = get_client_name (file);

	xdin = dict_new ();
	ret = dict_set_str (xdin, "clients", client);
	ret = dict_set_str (xdin, "query", "xfile");

	ret = syncop_ipc (xl, op, xdin, &xdout);
	if (ret)
		goto out;

	ret = file_fill_all_records (strfd, xdout);
out:
	if (xdin)
		dict_unref (xdin);

	return ret;
}

static int
imess_xfile_write (xlator_t *this, fd_t *fd, struct iovec *iov, int count)
{
	const char *str = NULL;

	str = iov[0].iov_base;

	return iov_length (iov, count);
}

struct meta_ops imess_xfile_ops = {
	.file_fill = imess_xfile_fill,
	.file_write = imess_xfile_write,
};

int
meta_imess_xfile_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_xfile_ops);

	return 0;
}

/*
 * xname
 */

static int
imess_xname_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	int ret = 0;
	int op = 0;
	xlator_t *xl = NULL;
	char *client = NULL;
	dict_t *xdin = NULL;
	dict_t *xdout = NULL;

	xl = this->children->xlator;
	client = get_client_name (file);

	xdin = dict_new ();
	ret = dict_set_str (xdin, "clients", client);
	ret = dict_set_str (xdin, "query", "xname");

	ret = syncop_ipc (xl, op, xdin, &xdout);
	if (ret)
		goto out;

	ret = file_fill_all_records (strfd, xdout);
out:
	if (xdin)
		dict_unref (xdin);

	return ret;
}

static int
imess_xname_write (xlator_t *this, fd_t *fd, struct iovec *iov, int count)
{
	const char *str = NULL;

	str = iov[0].iov_base;

	return iov_length (iov, count);
}

struct meta_ops imess_xname_ops = {
	.file_fill = imess_xname_fill,
	.file_write = imess_xname_write,
};

int
meta_imess_xname_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_xname_ops);

	return 0;
}

/*
 * xdata
 */

static int
imess_xdata_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	int ret = 0;
	int op = 0;
	xlator_t *xl = NULL;
	char *client = NULL;
	dict_t *xdin = NULL;
	dict_t *xdout = NULL;

	xl = this->children->xlator;
	client = get_client_name (file);

	xdin = dict_new ();
	ret = dict_set_str (xdin, "clients", client);
	ret = dict_set_str (xdin, "query", "xdata");

	ret = syncop_ipc (xl, op, xdin, &xdout);
	if (ret)
		goto out;

	ret = file_fill_all_records (strfd, xdout);
out:
	if (xdin)
		dict_unref (xdin);

	return ret;
}

static int
imess_xdata_write (xlator_t *this, fd_t *fd, struct iovec *iov, int count)
{
	const char *str = NULL;

	str = iov[0].iov_base;

	return iov_length (iov, count);
}

struct meta_ops imess_xdata_ops = {
	.file_fill = imess_xdata_fill,
	.file_write = imess_xdata_write,
};

int
meta_imess_xdata_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_xdata_ops);

	return 0;
}

/*
 * debug/imess-*
 */

static struct meta_dirent imess_index_dir_dirents[] = {
	DOT_DOTDOT,
	{ .name = "xfile",
	  .type = IA_IFREG,
	  .hook = meta_imess_xfile_hook,
	},
	{ .name = "xname",
	  .type = IA_IFREG,
	  .hook = meta_imess_xname_hook,
	},
	{ .name = "xdata",
	  .type = IA_IFREG,
	  .hook = meta_imess_xdata_hook,
	},
	{ .name = NULL }
};

struct meta_ops imess_index_dir_ops = {
	.fixed_dirents = imess_index_dir_dirents,
};

int
meta_imess_index_dir_hook (call_frame_t *frame, xlator_t *this,
				loc_t *loc, dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_index_dir_ops);

	return 0;
}

static int
imess_debug_dir_fill (xlator_t *this, inode_t *dir, struct meta_dirent **dp)
{
	struct meta_dirent *dirents = NULL;
	int i = 0;
	int count = 0;
	xlator_t *xl = NULL;

	/* each client would have a imess index db */
	for (xl = this; xl; xl = xl->next) {
		if (!strcmp(xl->type, "protocol/client"))
			count++;
	}

	dirents = GF_CALLOC (sizeof (*dirents), count, gf_meta_mt_dirents_t);
	if (!dirents)
		return -1;

	i = 0;
	for (xl = this; xl; xl = xl->next) {
		if (strcmp(xl->type, "protocol/client"))
			continue;

		dirents[i].name = gf_strdup (xl->name);
		dirents[i].type = IA_IFDIR;
		dirents[i].hook = meta_imess_index_dir_hook;
		i++;
	}

	*dp = dirents;
	return i;
}

/*
 * debug/all
 */

static struct meta_dirent imess_debug_all_dir_dirents[] = {
	DOT_DOTDOT,
	{ .name = "xfile",
	  .type = IA_IFREG,
	  .hook = meta_imess_xfile_hook,
	},
	{ .name = "xname",
	  .type = IA_IFREG,
	  .hook = meta_imess_xname_hook,
	},
	{ .name = "xdata",
	  .type = IA_IFREG,
	  .hook = meta_imess_xdata_hook,
	},
	{ .name = NULL },
};

struct meta_ops imess_debug_all_dir_ops = {
	.fixed_dirents = imess_debug_all_dir_dirents,
};

int
meta_imess_debug_all_dir_hook (call_frame_t *frame, xlator_t *this,
				loc_t *loc, dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_debug_all_dir_ops);

	return 0;
}

/*
 * debug/sql
 */

/** FIXME: this global will mess up everything */
static char sql[4096];

static int
imess_sql_query_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	strprintf (strfd, "%s\n", sql);

	return strfd->size;
}

static int
imess_sql_query_write (xlator_t *this, fd_t *fd, struct iovec *iov, int count)
{
	memcpy(sql, iov[0].iov_base, iov[0].iov_len);
	sql[iov[0].iov_len] = '\0';

	return iov_length (iov, count);
}

struct meta_ops imess_sql_query_ops = {
	.file_fill = imess_sql_query_fill,
	.file_write = imess_sql_query_write,
};

int
meta_imess_sql_query_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_sql_query_ops);

	return 0;
}

static int
imess_sql_result_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	int ret = 0;
	int op = 0;
	xlator_t *xl = NULL;
	dict_t *xdin = NULL;
	dict_t *xdout = NULL;

	xl = this->children->xlator;

        op = IMESS_IPC_OP;

	xdin = dict_new ();
	ret = dict_set_str (xdin, "clients", "all");
	ret = dict_set_str (xdin, "sql", sql);

	ret = syncop_ipc (xl, op, xdin, &xdout);
	if (ret)
		goto out;

	ret = file_fill_all_records (strfd, xdout);
out:
	if (xdin)
		dict_unref (xdin);

	return ret;
		goto out;

}

static int
imess_sql_result_write (xlator_t *this, fd_t *fd, struct iovec *iov, int count)
{
	return 0;
}

struct meta_ops imess_sql_result_ops = {
	.file_fill = imess_sql_result_fill,
	.file_write = imess_sql_result_write,
};

int
meta_imess_sql_result_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_sql_result_ops);

	return 0;
}

static struct meta_dirent imess_debug_sql_dir_dirents[] = {
	DOT_DOTDOT,
	{ .name = "query",
	  .type = IA_IFREG,
	  .hook = meta_imess_sql_query_hook,
	},
	{ .name = "result",
	  .type = IA_IFREG,
	  .hook = meta_imess_sql_result_hook,
	},
	{ .name = NULL },
};

struct meta_ops imess_debug_sql_dir_ops = {
	.fixed_dirents = imess_debug_sql_dir_dirents,
};

int
meta_imess_debug_sql_dir_hook (call_frame_t *frame, xlator_t *this,
				loc_t *loc, dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_debug_sql_dir_ops);

	return 0;
}

/*
 * debug dir
 */

static struct meta_dirent imess_debug_dirents[] = {
	DOT_DOTDOT,
	{ .name = "all",
	  .type = IA_IFDIR,
	  .hook = meta_imess_debug_all_dir_hook,
	},
	{ .name = "sql",
	  .type = IA_IFDIR,
	  .hook = meta_imess_debug_sql_dir_hook,
	},
	{ .name = NULL },
};


struct meta_ops imess_debug_dir_ops = {
	.fixed_dirents = imess_debug_dirents,
	.dir_fill = imess_debug_dir_fill,
};

int
meta_imess_debug_dir_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_debug_dir_ops);

	return 0;
}
