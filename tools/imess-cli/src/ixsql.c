/* Copyright (C) 2015 	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "ixsql.h"

#define PROMPT		"\nixsql> "

static ixsql_control_t  control;


static inline void welcome(void)
{
        printf("ixsql version 0.0.x. 'CTRL-D' to quit. good luck!\n");
}

static inline int print_result (ixsql_query_t *query)
{
        int ret             = 0;
        uint64_t i          = 0;
        uint64_t count      = 0;
        char keybuf[8]      = { 0, };
        char *row           = NULL;
	dict_t *xdata       = NULL;
	struct timeval *lat = NULL;

	xdata = query->result;
	lat = &query->latency;

        ret = dict_get_uint64 (xdata, "count", &count);
        if (ret)
                return -1;

        for (i = 0; i < count; i++) {
                sprintf (keybuf, "%llu", _llu (i));
                ret = dict_get_str (xdata, keybuf, &row);

                fprintf (control.fp_output, "[%7llu] %s\n", _llu (i+1), row);
        }

        fprintf (control.fp_output, "\n%llu records", _llu (count));
	fprintf (control.fp_output, ", %llu.%06llu seconds\n\n",
		     _llu (lat->tv_sec), _llu (lat->tv_usec));

        return 0;
}

static int process_sql (char *line)
{
        int ret               = 0;
	ixsql_query_t query   = { 0, };

	query.sql = line;

        ret = ixsql_sql_query (control.gluster, &query);
        if (ret)
                goto out;

        print_result (&query);

out:
        if (query.result)
                dict_destroy (query.result);

        return ret;
}

static void ixsql_shell(void)
{
        int ret = 0;
        char *line = NULL;

        welcome ();

        while (1) {
                line = readline (PROMPT);
                if (!line)
                        break;

                add_history (line);

		if (line[0] == '.')
			ret = ixsql_control_cmd (&control, line);
		else
			ret = process_sql (line);

		if (ret == IXSQL_QUIT)
			break;
		if (ret)
			perror ("something went wrong");
        }

        printf ("terminating..\n\n");
}

static struct option opts[] = {
	{ .name = "debug", .has_arg = 0, .flag = NULL, .val = 'd' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = 'h' },
	{ 0, 0, 0, 0 },
};

static void print_usage (void)
{
	printf ("Usage: ixsql [OPTS] <volname> <hostname>\n\n");
}

static int print_debug;

int main(int argc, char **argv)
{
        int ret    = 0;
	int op     = 0;
	glfs_t *fs = NULL;

	while ((op = getopt_long (argc, argv, "dh", opts, NULL)) != -1) {
		switch (op) {
		case 'd':
			print_debug = 1;
			break;
		case 'h':
		default:
			print_usage ();
			return 0;
		}
	}

	argv -= optind;
	argv += optind;

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
	if (print_debug)
		ret = glfs_set_logging (fs, "/dev/stderr", 7);
	else
		ret = glfs_set_logging (fs, "/dev/null", 7);
        ret = glfs_init (fs);

	control.gluster = fs;
	control.fp_output = stdout;

        fprintf (stderr, "glfs_init: returned %d\n", ret);

        ixsql_shell ();

        glfs_fini (fs);

        return ret;
}
