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

#include "server.h"

int32_t
init (xlator_t *this)
{
	return 0;
}

void
fini (xlator_t *this)
{
	return;
}

/*
struct xlator_fops fops = {
	{ .key = { NULL } },
};

struct xlator_cbks cbks = {
};

struct volume_options options [] = {
	{ .key = { NULL } },
};
*/

