#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "ix-imess.h"

static glfs_t *fs;

int main(int argc, char **argv)
{
        int ret = 0;

        if (argc != 3) {
                printf ("Expect following args\n\t%s <volname> <hostname>\n",
                                argv[0]);
                return -1;
        }

        fs = glfs_new (argv[1]);
        if (!fs) {
                fprintf (stderr, "glfs_new: returned NULL\n");
                return 1;
        }

        ret = glfs_set_volfile_server (fs, "tcp", argv[2], 24007);
        ret = glfs_set_logging (fs, "/dev/stderr", 7);
        ret = glfs_init (fs);

        fprintf (stderr, "glfs_init: returned %d\n", ret);

        glfs_fini (fs);

        return ret;
}
