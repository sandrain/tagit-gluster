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

static struct meta_dirent analyzethis_self_dirents[] = {
	DOT_DOTDOT,
};

struct meta_ops analyzethis_self_dir_ops = {
	.fixed_dirents = analyzethis_self_dirents,
};


int
meta_analyzethis_self_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &analyzethis_self_dir_ops);

	return 0;
}