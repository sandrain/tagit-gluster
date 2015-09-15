/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _IMESS_SERVER_H_
#define	_IMESS_SERVER_H_

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "glusterfs.h"
#include "xlator.h"
#include "common-utils.h"
#include "logging.h"
#include "options.h"
#include "dict.h"

#include "imess-server-mem-types.h"
#include "imess-server-xdb.h"

struct _ims_priv {
	char          *db_path;
	ims_xdb_t     *xdb;

	gf_boolean_t  lookup_cache;
};

typedef struct _ims_priv ims_priv_t;

/*
 * operation code for ipc. the actual command comes as string in xdata
 */

#endif	/* _IMESS_SERVER_H_ */
