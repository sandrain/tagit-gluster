/*
   Copyright (c) 2014 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "xlator.h"
#include "defaults.h"

#include "meta-mem-types.h"
#include "meta.h"
#include "meta-hooks.h"


static struct meta_dirent analyzethis_dir_dirents[] = {
	DOT_DOTDOT,

	{ .name = "submit",
	  .type = IA_IFREG,
	  .hook = meta_analyzethis_submit_hook,
	},
	{ .name = "jobs",
	  .type = IA_IFDIR,
	  .hook = meta_analyzethis_jobs_hook,
	},
	{ .name = "self",
	  .type = IA_IFDIR,
	  .hook = meta_analyzethis_self_hook,
	},
	{ .name = NULL }
};


struct meta_ops analyzethis_dir_ops = {
	.fixed_dirents = analyzethis_dir_dirents,
};

int
meta_analyzethis_dir_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
		     dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &analyzethis_dir_ops);

	return 0;
}

