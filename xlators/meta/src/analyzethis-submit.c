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

static int
analyzethis_submit_file_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	strprintf (strfd, "Usage: echo /path/to/script.job > submit\n");

	return strfd->size;
}

static int
analyzethis_submit_file_write (xlator_t *this, fd_t *fd,
				struct iovec *iov, int count)
{
	const char *str = NULL;

	str = iov[0].iov_base;

	return iov_length (iov, count);
}


struct meta_ops analyzethis_submit_ops = {
	.file_fill = analyzethis_submit_file_fill,
	.file_write = analyzethis_submit_file_write,
};


int
meta_analyzethis_submit_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &analyzethis_submit_ops);

	return 0;
}

