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
#include <assert.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "ixsql.h"

#define PROMPT		"\nixsql> "

static ixsql_control_t  control;
static char qbuf[4096];

static inline void welcome(void)
{
        printf("ixsql version 0.0.x. 'CTRL-D' to quit. good luck!\n");
}

static void print_single_row (FILE *fp, char *row)
{
	fprintf (fp, "%s\n", row);
}

static int read_client_number (glfs_t *fs, ixsql_control_t *ctl)
{
	int count = 0;
	char *path = NULL;
	glfs_fd_t *fd = NULL;
        char buf[512] = { 0, };
        struct dirent *entry = NULL;

	sprintf (qbuf, "/.meta/graphs/active/%s-dht/subvolumes", ctl->volname);
	path = qbuf;

	fd = glfs_opendir (fs, path);
	if (!fd) {
		fprintf (stderr, "failed to open %s: %s\n",
				 path, strerror (errno));
		return -1;
	}

	while (glfs_readdir_r (fd, (struct dirent *) buf, &entry), entry)
		count++;

	return count;
}

static int64_t print_result (ixsql_query_t *query)
{
        int ret             = 0;
        uint64_t i          = 0;
        uint64_t count      = 0;
        char keybuf[12]     = { 0, };
        char *row           = NULL;
	dict_t *xdata       = NULL;

	xdata = query->result;

        ret = dict_get_uint64 (xdata, "count", &count);
        if (ret)
                return -1;

        for (i = 0; i < count; i++) {
                sprintf (keybuf, "%llu", _llu (i));
                ret = dict_get_str (xdata, keybuf, &row);

		print_single_row (control.fp_output, row);
        }

        return count;
}

static int process_sql_sliced (ixsql_query_t *query)
{
	int ret                = 0;
	int offset             = 0;
	int count              = 0;
	int64_t res_count      = 0;
	ixsql_query_t my_query = { 0, };

	count = control.slice_count;

	do {
		sprintf (qbuf, "%s limit %d,%d", query->sql, offset, count);

		my_query.sql = qbuf;
		ret = ixsql_sql_query (&control, &my_query);
		if (ret)
			goto out;

		res_count = print_result (&my_query);
		if (res_count == -1)
			goto out;

		offset += count;
	} while (res_count > 0);

out:
	if (my_query.result)
		dict_destroy (my_query.result);

	return ret;
}

static int process_sql (char *line)
{
        int ret               = 0;
	ixsql_query_t query   = { 0, };
	struct timeval before = { 0, };
	struct timeval after  = { 0, };
	struct timeval lat    = { 0, };

	query.sql = line;

	gettimeofday (&before, NULL);

	if (control.slice_count == 0) {
		ret = ixsql_sql_query (&control, &query);
		if (ret)
			goto out;

		print_result (&query);

	}
	else {
		ret = process_sql_sliced (&query);
		if (ret)
			goto out;
	}

	gettimeofday (&after, NULL);

	if (!control.direct || control.show_latency) {
		timeval_latency (&lat, &before, &after);
		fprintf (stdout, "## %llu.%06llu seconds\n",
				_llu (lat.tv_sec), _llu (lat.tv_usec));
	}

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
	{ .name = "client", .has_arg = 1, .flag = NULL, .val = 'c' },
	{ .name = "latency", .has_arg = 0, .flag = NULL, .val = 'l' },
	{ .name = "mute", .has_arg = 0, .flag = NULL, .val = 'm' },
	{ .name = "sql", .has_arg = 1, .flag = NULL, .val = 'q' },
	{ .name = "slice", .has_arg = 1, .flag = NULL, .val = 's' },
	{ 0, 0, 0, 0 },
};

static const char *usage_str = 
"\n"
"usage: xsql [options..] <volume id> <volume server>\n"
"\n"
"options:\n"
"  --debug, -d            print log messages to stderr\n"
"  --help, -h             this help message\n"
"  --sql, -q [sql query]  execute sql directly\n"
"  --slice, -s [N]        fetch [N] result per a query (with -q option)\n"
"  --latency, -l          show query latency (with -q option)\n"
"  --mute, -m             mute output, useful for measuring the latency\n"
"  --client, -c [N]       send query to a specific client\n"
"\n\n";

static void print_usage (void)
{
	fputs (usage_str, stdout);
}

static int print_debug;

int main(int argc, char **argv)
{
        int ret              = 0;
	int op               = 0;
	int mute             = 0;
	int show_latency     = 0;
	int direct           = 0;
	int n_clients        = 0;
	uint64_t slice_count = IXSQL_DEFAULT_SLICE;
	int active_client    = -1;
	char *sql            = NULL;
	glfs_t *fs           = NULL;

	while ((op = getopt_long (argc, argv, "c:dhlmq:s:", opts, NULL))
			!= -1) {
		switch (op) {
		case 'c':
			active_client = atoi (optarg);
			break;
		case 'd':
			print_debug = 1;
			break;
		case 'l':
			show_latency = 1;
			break;
		case 'm':
			mute = 1;
			break;
		case 'q':
			direct = 1;
			sql = optarg;
			break;
		case 's':
			slice_count = (uint64_t ) strtoul (optarg, NULL, 0);
			break;
		case 'h':
		default:
			print_usage ();
			return -1;
		}
	}

	argc -= optind;
	argv += optind;

        if (argc != 2) {
		print_usage ();
                return -1;
        }

        fs = glfs_new (argv[0]);
        if (!fs) {
                fprintf (stderr, "glfs_new: returned NULL\n");
                return 1;
        }

        ret = glfs_set_volfile_server (fs, "tcp", argv[1], 24007);
	if (print_debug)
		ret = glfs_set_logging (fs, "/dev/stderr", 7);
	else
		ret = glfs_set_logging (fs, "/dev/null", 7);
        ret = glfs_init (fs);

	control.gluster = fs;
	control.volname = argv[0];
	control.volserver = argv[1];

	n_clients = read_client_number (fs, &control);
	control.num_clients = n_clients;
	control.active_client = active_client;

	if (mute) {
		control.fp_output = fopen("/dev/null", "w");
		assert(control.fp_output);
	}
	else
		control.fp_output = stdout;
	control.slice_count = slice_count;
	control.show_latency = show_latency;
	control.direct = direct;

	if (direct)
		process_sql (sql);
	else
		ixsql_shell ();

        glfs_fini (fs);

        return ret;
}
