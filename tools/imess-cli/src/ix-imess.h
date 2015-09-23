#ifndef _IX_IMESS_H_
#define _IX_IMESS_H_

#include "glfs.h"
#include "glfs-handles.h"

/* gluster internal headers */
#undef _CONFIG_H
#include "glusterfs.h"
#include "xlator.h"

#include <sys/time.h>

static inline void
timeval_latency (struct timeval *out,
		 struct timeval *before, struct timeval *after)
{
	time_t sec       = 0;
	suseconds_t usec = 0;

	if (!out || !before || !after)
		return;

	sec = after->tv_sec - before->tv_sec;
	if (after->tv_usec < before->tv_usec) {
		sec -= 1;
		usec = 1000000 + after->tv_usec - before->tv_usec;
	}
	else
		usec = after->tv_usec - before->tv_usec;

	out->tv_sec = sec;
	out->tv_usec = usec;
}


#endif	/* _IX_IMESS_H_ */
