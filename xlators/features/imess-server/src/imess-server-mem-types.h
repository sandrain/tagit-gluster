#ifndef __IMESS_SERVER_MEM_TYPES_H__
#define __IMESS_SERVER_MEM_TYPES_H__

#include "mem-types.h"

enum gf_imess_server_mem_types_ {
        gf_ims_mt_ims_priv_t = gf_common_mt_end + 1,
        gf_ims_mt_ims_xdb_t,
	gf_ims_mt_ims_async_t,
        gf_ims_mt_ims_task_t,
	gf_ims_mt_end
};

#endif
