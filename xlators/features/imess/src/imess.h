/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _IMESS_H_
#define	_IMESS_H_

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "imess-mem-types.h"
#include "glusterfs.h"
#include "xlator.h"
#include "common-utils.h"
#include "logging.h"
#include "options.h"

#include "xdb.h"

/* recovery mode */
enum {
	IMESS_COMMIT_LAZY		= 0,	/* no commit until unmount */
	IMESS_COMMIT_SYNC,			/* only for journal commit */
	IMESS_COMMIT_PARANOID,			/* crazy, commit all */
	IMESS_COMMIT_DYNAMIC,			/* dynamic to the load */
};

typedef struct {
	char          *dbpath;
	xdb_t         *xdb;
	int            commit_mode;
	gf_boolean_t   lookup_cache;
} imess_priv_t;

#define IMESS_STACK_UNWIND(op, frame, params ...)                       \
        do {                                                            \
                frame->local = NULL;                                    \
                STACK_UNWIND_STRICT (op, frame, params);                \
        } while (0);

#endif	/* _IMESS_H_ */

