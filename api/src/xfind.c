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
#include "xlator.h"

#define PROMPT		"xfind> "
#define	LINEBUFSIZE	4096

static glfs_t *fs;
static glfs_fd_t *fd;
static char linebuf[LINEBUFSIZE];

static dict_t *cmd;
static dict_t *result;

#ifndef _llu
#define _llu(x)	((unsigned long long) (x))
#endif

static inline int print_result (FILE *fp, dict_t *xdata)
{
	int ret = 0;
	uint64_t i = 0;
	uint64_t count = 0;
	char keybuf[8] = { 0, };
	char *row = NULL;

	ret = dict_get_uint64 (xdata, "count", &count);
	if (ret)
		return -1;

	for (i = 0; i < count; i++) {
		sprintf(keybuf, "%llu", _llu(i));
		ret = dict_get_str (xdata, keybuf, &row);

		fprintf(fp, "[%7llu] %s\n", _llu(i+1), row);
	}

	fprintf(fp, "\n%llu records\n", _llu(count));

	return 0;
}

static int process_query(char *line)
{
	int ret = 0;

	if (!cmd) {
		cmd = dict_new();
		if (!cmd) {
			ret = -1;
			goto out;
		}

		ret = dict_set_str(cmd, "clients", "all");
		ret = dict_set_str(cmd, "sql", line);

		fd = glfs_opendir (fs, "/");
		if (!fd) {
			ret = -1;
			goto out;
		}
                glfs_closedir (fd);
	}

	ret = glfs_ipc (fs, IMESS_IPC_OP, cmd, &result);
        if (ret)
                goto out;

        print_result (stdout, result);

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
		if (!line || !strncmp("quit", line, strlen("quit")))
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
