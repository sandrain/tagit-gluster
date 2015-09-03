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
	IMESS_REC_NONE		= 0,	/* no commit until unmount */
	IMESS_REC_PESS,			/* pessimistic: commit on each ops */
	IMESS_REC_FRAC,			/* commit on each XX ops */
	IMESS_REC_SYNC,			/* commit on each sync ops */
};

typedef struct {
	char          *dbpath;
	xdb_t         *xdb;
	int            recovery_mode;
	gf_boolean_t   lookup_cache;
} imess_priv_t;

#define IMESS_STACK_UNWIND(op, frame, params ...)                       \
        do {                                                            \
                frame->local = NULL;                                    \
                STACK_UNWIND_STRICT (op, frame, params);                \
        } while (0);

#endif	/* _IMESS_H_ */

