#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "glfs.h"
#include "glfs-handles.h"
#include <string.h>
#include <time.h>

/* gluster internal headers */
#undef _CONFIG_H
#include "glusterfs.h"

#define PROMPT		"xfind> "
#define	LINEBUFSIZE	4096

static glfs_t *fs;
static glfs_fd_t *fd;
static char linebuf[LINEBUFSIZE];

static dict_t *cmd;
static dict_t *result;

static int process_query(const char *line)
{
	int ret = 0;

	if (!cmd) {
		cmd = dict_new();
		if (!cmd) {
			ret = -1;
			goto out;
		}

		ret = dict_set_str(cmd, "clients", "all");
		ret = dict_set_str(cmd, "table", "line");

		fd = glfs_opendir (fs, "/");
		if (!fd) {
			ret = -1;
			goto out;
		}
	}

	ret = glfs_ipc(fd, 0, cmd, &result);

out:
	return ret;
}

static inline void welcome(void)
{
	printf("xfind version 0.0.x. 'quit' to finish, good luck!\n\n");
}

static void xfind_shell(void)
{
	int ret = 0;
	char *line = NULL;

	welcome();

	while (1) {
		fputs(PROMPT, stdout);
		fflush(stdout);

		line = fgets(linebuf, LINEBUFSIZE-1, stdin);
		if (!line || strncmp("quit", line, strlen("quit")))
			break;

		if ((ret = process_query(line)) != 0)
			break;
	}

	perror("terminating..");
}

int main(int argc, char **argv)
{
        int ret = 0;

        if (argc != 3) {
                printf ("Expect following args\n\t%s <volname> <hostname>\n", argv[0]);
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

	xfind_shell();

        glfs_fini (fs);

        return ret;
}
