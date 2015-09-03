#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "xlator.h"
#include "defaults.h"

#include "meta-mem-types.h"
#include "meta.h"

static int
imess_create_view_file_fill (xlator_t *this, inode_t *file, strfd_t *strfd)
{
	strprintf (strfd, "Usage: echo /path/to/script.job > submit\n");

	return strfd->size;
}

static int
imess_create_view_file_write (xlator_t *this, fd_t *fd,
				struct iovec *iov, int count)
{
	const char *str = NULL;

	str = iov[0].iov_base;

	return iov_length (iov, count);
}


struct meta_ops imess_create_view_ops = {
	.file_fill = imess_create_view_file_fill,
	.file_write = imess_create_view_file_write,
};


int
meta_imess_create_view_hook (call_frame_t *frame, xlator_t *this, loc_t *loc,
			dict_t *xdata)
{
	meta_ops_set (loc->inode, this, &imess_create_view_ops);

	return 0;
}


