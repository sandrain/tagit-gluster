/* Copyright (C) 2015 	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "ixsql.h"

#define PROMPT		"\nixsql> "

static int verbose;
static ixsql_control_t  control;
static char qbuf[4096];

static int terminate;
static struct timeval before;
static struct timeval after;
static struct timeval latency;

static void alarm_handler (int signal)
{
	if (signal == SIGALRM)
		terminate = 1;
}

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

	query.sql = line;

	/*
	 * we only slice for the select query
	 * FIXME: this comparison is case-sensitive, cannot catch 'SELECT' or
	 * 'Select'
	 */
	if (strncmp ("select", line, strlen("select"))
	    || control.slice_count == 0)
	{
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

out:
        if (query.result)
                dict_destroy (query.result);

        return ret;
}

static uint64_t process_batch (char *file)
{
	int ret = 0;
	uint64_t count = 0;
	FILE *fp = NULL;
	char linebuf[4096] = { 0, };

	fp = fopen (file, "r");
	if (fp == NULL) {
		perror ("fopen");
		return -1;
	}

	while (fgets (linebuf, 4095, fp) != NULL) {
		if (linebuf[0] == '#' || isspace(linebuf[0]))
			continue;

		ret = process_sql (linebuf);
		if (ret)
			goto out;

		count++;
		fprintf (control.fp_output, "processed: %10llu\r",
			 _llu(count));
	}
	if (ferror (fp)) {
		perror ("fgets");
		ret = -1;
	}

	fputc('\n', control.fp_output);
out:
	fclose (fp);

	return count;
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
		else {
			gettimeofday (&before, NULL);

			ret = process_sql (line);

			gettimeofday (&after, NULL);
			timeval_latency (&latency, &before, &after);

			fprintf (control.fp_output,
				 "## elapsed: %llu.%06llu sec\n",
				 _llu (latency.tv_sec),
				 _llu (latency.tv_usec));
		}

		if (ret == IXSQL_QUIT)
			break;
		if (ret)
			perror ("something went wrong");
        }

        printf ("terminating..\n\n");
}

static struct option opts[] = {
	{ .name = "benchmark", .has_arg = 1, .flag = NULL, .val = 'b' },
	{ .name = "debug", .has_arg = 0, .flag = NULL, .val = 'd' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "client", .has_arg = 1, .flag = NULL, .val = 'c' },
	{ .name = "latency", .has_arg = 0, .flag = NULL, .val = 'l' },
	{ .name = "mute", .has_arg = 0, .flag = NULL, .val = 'm' },
	{ .name = "sql", .has_arg = 1, .flag = NULL, .val = 'q' },
	{ .name = "slice", .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "sql-file", .has_arg = 1, .flag = NULL, .val ='f' },
	{ .name = "verbose", .has_arg = 1, .flag = NULL, .val = 'v' },
	{ 0, 0, 0, 0 },
};

static const char *usage_str = 
"\n"
"usage: xsql [options..] <volume id> <volume server>\n"
"\n"
"options:\n"
"  --benchmark, -b [sec]  benchmark mode for [sec], should be used with -q\n"
"  --debug, -d            print log messages to stderr\n"
"  --help, -h             this help message\n"
"  --sql, -q [sql query]  execute sql directly\n"
"  --slice, -s [N]        fetch [N] result per a query (with -q option)\n"
"  --latency, -l          show query latency (with -q option)\n"
"  --mute, -m             mute output, useful for measuring the latency\n"
"  --client, -c [N]       send query to a specific client\n"
"  --sql-file, -f [file]  batch execute queries in [file]\n"
"  --verbose, -v          show more information including error codes\n"
"\n\n";

static void print_usage (void)
{
	fputs (usage_str, stdout);
}

static int print_debug;

int main(int argc, char **argv)
{
        int ret               = 0;
	int op                = 0;
	int mute              = 0;
	int benchmark         = 0;
	unsigned int duration = 0;
	int show_latency      = 0;
	int direct            = 0;
	int n_clients         = 0;
	uint64_t slice_count  = IXSQL_DEFAULT_SLICE;
	int active_client     = -1;
	char *sql             = NULL;
	char *sql_file        = NULL;
	glfs_t *fs            = NULL;
	double elapsed_sec    = 0.0F;
	uint64_t count        = 0;

	while ((op = getopt_long (argc, argv, "b:c:dhlmq:s:f:v", opts, NULL))
			!= -1) {
		switch (op) {
		case 'b':
			benchmark = 1;
			duration = atol (optarg);
			break;
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
		case 'f':
			sql_file = optarg;
			/* fall down to direct mode with sql_file */
		case 'q':
			direct = 1;
			sql = sql_file ? NULL : optarg;
			break;
		case 's':
			slice_count = (uint64_t ) strtoul (optarg, NULL, 0);
			break;
		case 'v':
			verbose = 1;
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

	if (benchmark) {
		if (!sql) {
			fprintf (stderr, "sql query should be given for "
					 "benchmark mode\n");
			return 1;
		}

		if (duration < 1) {
			fprintf (stderr, "are you serious? (%llu sec)\n",
					 _llu (duration));
			return 1;
		}

		direct = 1;
		mute = 1;
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

	if (benchmark) {
		signal (SIGALRM, alarm_handler);
		assert (0 == alarm (duration));

		gettimeofday (&before, NULL);

		while (!terminate) {
			process_sql (sql);
			count++;
		}

		gettimeofday (&after, NULL);
		timeval_latency (&latency, &before, &after);
		elapsed_sec = timeval_to_sec (&latency);
	}
	else if (direct) {
		gettimeofday (&before, NULL);

		if (sql_file)
			count = process_batch (sql_file);
		else {
			process_sql (sql);
			count = 1;
		}

		gettimeofday (&after, NULL);
		timeval_latency (&latency, &before, &after);
		elapsed_sec = timeval_to_sec (&latency);
	}
	else
		ixsql_shell ();

	if (direct)
		fprintf (stdout,
			 "## %llu queries were processed. %.3lf ops/sec\n",
			 _llu (count), 1.0 * count / elapsed_sec);

        glfs_fini (fs);

        return ret;
}
