/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef _IMESS_CLIENT_H_
#define	_IMESS_CLIENT_H_

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"
#include "options.h"
#include "defaults.h"
#include "dict.h"

#include "imess-client-mem-types.h"

struct _imc_priv {
	gf_boolean_t   dynamic_load_control;
};

typedef struct _imc_priv imc_priv_t;

#endif	/* _IMESS_CLIENT_H_ */

