#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "xlator.h"
#include "defaults.h"

#include "meta-mem-types.h"
#include "meta.h"
#include "meta-hooks.h"

static struct meta_dirent imess_dir_dirents[] = {
	DOT_DOTDOT,
	{ .name = "views",
	  .type = IA_IFDIR,
	  .hook = meta_imess_views_dir_hook,
	},
	{ .name = "tags",
	  .type = IA_IFDIR,
	  .hook = meta_imess_tags_dir_hook,
	},
	{ .name = NULL }
};

struct meta_ops imess_dir_ops = {
	.fixed_dirents = imess_dir_dirents,
};

int
meta_imess_dir_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
		     dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_dir_ops);

	return 0;
}

