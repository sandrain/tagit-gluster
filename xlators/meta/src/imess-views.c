#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "xlator.h"
#include "defaults.h"
#include "syncop.h"

#include "meta-mem-types.h"
#include "meta.h"

/* imess-views-db.c */
int imess_viewdb_getlist (dict_t *data);
int imess_viewdb_get_associated_sql (const char *name, char **sql);

/*
 * link files
 */

static int
imess_view_link_fill (xlator_t *this, inode_t *inode, strfd_t *strfd)
{
	char *path = NULL;

	path = meta_ctx_get(inode, this);
	strprintf (strfd, "../../../..%s", path);

	return 0;
}

struct meta_ops imess_view_link_ops = {
	.link_fill = imess_view_link_fill
};

int
meta_imess_view_link_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			   dict_t *xdata)
{
	int ret      = 0;
	dict_t *data = NULL;
	char *path   = NULL;

	data = meta_ctx_get (loc->parent, this);
	ret = dict_get_str (data, (char *) loc->name, &path);
	if (ret)
		return -1;

	meta_ctx_set (loc->inode, this, (void *) path);
	meta_ops_set (loc->inode, this, &imess_view_link_ops);

	return 0;
}


/*
 * individual view dir
 */

static struct meta_dirent imess_view_dir_dirents[] = {
	DOT_DOTDOT,
	{ .name = NULL }
};

static int
imess_view_dir_fill (xlator_t *this, inode_t *inode, struct meta_dirent **dp)
{
	int ret                     = 0;
	uint64_t i                  = 0;
	uint64_t count              = 0;
	char keybuf[16]             = { 0, };
	dict_t *data                = NULL;
	struct meta_dirent *dirents = NULL;

	data = meta_ctx_get (inode, this);

	ret = dict_get_uint64 (data, "count", &count);
	if (ret)
		return -1;

	if (!count)
		return 0;

	dirents = GF_CALLOC (sizeof (*dirents), count, gf_meta_mt_dirents_t);
	if (!dirents)
		return -1;

	for (i = 0; i < count; i++) {
		sprintf (keybuf, "%llu", (unsigned long long) i);

		dirents[i].name = gf_strdup (keybuf);
		dirents[i].type = IA_IFLNK;
		dirents[i].hook = meta_imess_view_link_hook;
	}

	*dp = dirents;

	return (int) count;
}

struct meta_ops imess_view_dir_ops = {
	.fixed_dirents = imess_view_dir_dirents,
	.dir_fill = imess_view_dir_fill,
};

int
meta_imess_view_dir_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
				dict_t *xdata)
{
	int ret          = 0;
	char *sql        = NULL;
	dict_t *data     = NULL;
	dict_t *data_in  = NULL;
	dict_t *data_out = NULL;

	data = meta_ctx_get (loc->parent, this);
	ret = dict_get_str (data, (char *) loc->name, &sql);
	if (ret)
		return -1;

	data_in = dict_new ();
	if (!data_in)
		return -1;

	ret  = dict_set_str (data_in, "type", "query");
	ret |= dict_set_str (data_in, "sql", sql);
	ret |= dict_set_str (data_in, "clients", "all");
	if (ret)
		return -1;

	ret = syncop_ipc (FIRST_CHILD (this), IMESS_IPC_OP,
				data_in, &data_out);
	if (ret)
		return -1;

	meta_ctx_set (loc->inode, this, (void *) data_out);
	meta_ops_set (loc->inode, this, &imess_view_dir_ops);

	return 0;
}

/*
 * views rootdir
 */

static struct meta_dirent imess_views_dir_dirents[] = {
	DOT_DOTDOT,
	{ .name = NULL }
};

struct views_dir_filler_data {
	int index;
	struct meta_dirent *dirents;
};

static int
views_dir_filler (dict_t *dict, char *key, data_t *value, void *arg)
{
	struct views_dir_filler_data *data = arg;
	struct meta_dirent *dirent         = &data->dirents[data->index];

	data->index++;

	dirent->name = gf_strdup (key);
	dirent->type = IA_IFDIR;
	dirent->hook = meta_imess_view_dir_hook;

	return 0;
}

static int
imess_views_dir_fill (xlator_t *this, inode_t *inode, struct meta_dirent **dp)
{
	int ret = 0;
	int count = 0;
	dict_t *data = NULL;
	struct meta_dirent *dirents = NULL;
	struct views_dir_filler_data filler_data = { 0, };

	data = meta_ctx_get (inode, this);
	count = data->count;	/* FIXME: probably this is not safe */

	dirents = GF_CALLOC (sizeof (*dirents), count, gf_meta_mt_dirents_t);
	if (!dirents)
		return -1;

	filler_data.index = 0;
	filler_data.dirents = dirents;

	ret = dict_foreach (data, views_dir_filler, (void *) &filler_data);
	if (ret)
		return -1;

	*dp = dirents;

	return count;
}

struct meta_ops imess_views_dir_ops = {
	.fixed_dirents = imess_views_dir_dirents,
	.dir_fill = imess_views_dir_fill,
};

int
meta_imess_views_dir_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
				dict_t *xdata)
{
	int ret      = 0;
	dict_t *data = NULL;

	data = dict_new ();
	if (!data)
		return -1;

	ret = imess_viewdb_getlist (data);

	meta_ops_set (loc->inode, this, &imess_views_dir_ops);
	meta_ctx_set (loc->inode, this, (void *) data);

	return 0;
}

