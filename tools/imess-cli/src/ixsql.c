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
#include <strings.h>
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
static ixsql_control_t  *control;
static char qbuf[4096];

/* NOTE: don't do slicing for the active executions */
static char *operator;

static int terminate;
static struct timeval before;
static struct timeval after;
static struct timeval latency;

static void sig_handler (int signal)
{
	if (signal == SIGALRM)
		terminate = 1;
}

static inline void welcome(void)
{
        printf("ixsql version 0.0.x. 'CTRL-D' to quit. good luck!\n");
}

static ixsql_control_t *init_control (glfs_t *fs, char *volname,
				      char *volserver, int n_clients)
{
	int i = 0;
	ixsql_control_t *ctl = NULL;

	ctl = malloc (sizeof(*ctl) + sizeof(char)*n_clients);
	memset ((void *) ctl, 0, sizeof(*ctl) + sizeof(char)*n_clients);

	if (ctl) {
		for (i = 0; i < n_clients; i++)
			ctl->cli_mask[i] = 1;

		ctl->num_clients = n_clients;
		ctl->active_clients = n_clients;
		ctl->gluster = fs;
		ctl->volname = volname;
		ctl->volserver = volserver;
	}

	return ctl;
}

static int read_client_number (glfs_t *fs, char *volname)
{
	int count = 0;
	char *path = NULL;
	glfs_fd_t *fd = NULL;
        char buf[512] = { 0, };
        struct dirent *entry = NULL;

	sprintf (qbuf, "/.meta/graphs/active/%s-dht/subvolumes", volname);
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

static int
print_metadata_fn (dict_t *dict, char *key, data_t *value, void *data)
{
	int ret        = 0;
	int32_t op_ret = 0;
	uint64_t count = 0;
	double runtime = .0F;
	char *pos      = NULL;

	if (NULL != (pos = strstr (key, ":ret"))) {
		op_ret = data_to_int32 (value);
		fprintf (control->fp_output, "## %s = %d\n", key, op_ret);
	}
	else if (NULL != (pos = strstr (key, ":count"))) {
		count = data_to_uint64 (value);
		fprintf (control->fp_output, "## %s = %llu\n",
					     key, _llu (count));
	}
	else if (NULL != (pos = strstr (key, ":runtime"))) {
		ret = dict_get_double (dict, key, &runtime);
		fprintf (control->fp_output, "## %s = %.6f\n", key, runtime);
	}
	else {
		/* no idea, ignore for now. */
	}

	return 0;
}

static inline int print_data (ixsql_query_t *query)
{
	int ret              = 0;
	uint64_t i           = 0;
	uint64_t count       = 0;
        char keybuf[16]      = { 0, };
	char *row            = NULL;
	dict_t *result       = NULL;

	count = query->count;
	result = query->result;

	for (i = 0; i < count; i++) {
		sprintf (keybuf, "%llu", _llu (i));
                ret = dict_get_str (result, keybuf, &row);
		fputs (row, control->fp_output);
	}

	return 0;
}

static inline int64_t print_better_result (ixsql_query_t *query)
{
	int ret        = 0;
	uint64_t count = 0;
	dict_t *result = NULL;

	result = query->result;

	ret = dict_get_uint64 (result, "count", &count);
	if (ret)
		goto err;

	query->count = count;

	if (verbose)
		ret = dict_foreach_fnmatch (result, "*:*",
					    print_metadata_fn, query);

	ret = print_data (query);
err:
	if (ret) {
		fprintf (stderr, "cannot parse the output (%d)\n", ret);
		ret = -1;
	}

	return ret;
}

static inline int process_sql_sliced (ixsql_query_t *query)
{
	int ret                = 0;
	int offset             = 0;
	int count              = 0;
	int64_t res_count      = 0;
	ixsql_query_t my_query = { 0, };

	count = control->slice_count;

	do {
		sprintf (qbuf, "%s limit %d,%d", query->sql, offset, count);

		my_query.sql = qbuf;
		ret = ixsql_sql_query (control, &my_query);
		if (ret)
			goto out;

		res_count = print_better_result (&my_query);
		if (res_count == -1)
			goto out;

		offset += count;
	} while (res_count > 0);

out:
	if (my_query.result)
		dict_destroy (my_query.result);

	return ret;
}

static inline int process_sql (char *line)
{
        int ret               = 0;
	ixsql_query_t query   = { 0, };

	query.sql = line;

	/*
	 * we only slice for the select query
	 * FIXME: this comparison is case-sensitive, cannot catch 'SELECT' or
	 * 'Select'
	 */
	if (strncasecmp ("select", line, strlen("select"))
	    || control->slice_count == 0)
	{
		ret = ixsql_sql_query (control, &query);
		if (ret)
			goto out;

		print_better_result (&query);
	}
	else
		ret = process_sql_sliced (&query);
out:
        if (query.result)
                dict_destroy (query.result);

        return ret;
}

static inline int process_exec (char *line)
{
        int ret               = 0;
	ixsql_query_t query   = { 0, };

	/* FIXME: make the comparison more intelligent (dealing with spaces) */
	if (strncasecmp (line, "select path from",
			 strlen ("select path from")))
	{
		fprintf (stderr, "the query should fetch 'path' for -exec");
		return -1;
	}

	control->slice_count = 0;
	query.sql = line;
	query.operator = operator;

	ret = ixsql_exec (control, &query);
	if (ret)
		goto out;

	print_better_result (&query);
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
		fprintf (control->fp_output, "processed: %10llu\r",
			 _llu(count));
	}
	if (ferror (fp)) {
		perror ("fgets");
		ret = -1;
	}

	fputc('\n', control->fp_output);
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
			ret = ixsql_control_cmd (control, line);
		else {
			gettimeofday (&before, NULL);

			ret = process_sql (line);

			gettimeofday (&after, NULL);
			timeval_latency (&latency, &before, &after);

			fprintf (control->fp_output,
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
	{ .name = "client", .has_arg = 1, .flag = NULL, .val = 'c' },
	{ .name = "debug", .has_arg = 0, .flag = NULL, .val = 'd' },
	{ .name = "exec", .has_arg = 1, .flag = NULL, .val = 'x' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "latency", .has_arg = 0, .flag = NULL, .val = 'l' },
	{ .name = "mute", .has_arg = 0, .flag = NULL, .val = 'm' },
	{ .name = "num-clients", .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "slice", .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "sql", .has_arg = 1, .flag = NULL, .val = 'q' },
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
"  --exec, -x [operator]  active execution for the result files\n"
"  --help, -h             this help message\n"
"  --sql, -q [sql query]  execute sql directly\n"
"  --slice, -s [N]        fetch [N] result per a query (with -q option)\n"
"  --latency, -l          show query latency (with -q option)\n"
"  --mute, -m             mute output, useful for measuring the latency\n"
"  --client, -c [N]       send query to a specific clients \n"
"  --num-clients, -n [N]  number of clients to be used (for benchmark)\n"
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
	int n_test_clients    = 0;
	uint64_t slice_count  = IXSQL_DEFAULT_SLICE;
	int client            = -1;
	char *sql             = NULL;
	char *sql_file        = NULL;
	glfs_t *fs            = NULL;
	uint64_t count        = 0;
	double elapsed_sec    = 0.0F;

	while ((op = getopt_long (argc, argv,
				  "b:c:df:hlmn:q:s:vx:",
				  opts, NULL)) != -1)
	{
		switch (op) {
		case 'b':
			benchmark = 1;
			duration = atol (optarg);
			break;
		case 'c':
			client = atoi (optarg);
			break;
		case 'd':
			print_debug = 1;
			break;
		case 'v':
			verbose = 1;
		case 'l':
			show_latency = 1;
			break;
		case 'm':
			mute = 1;
			break;
		case 'n':
			n_test_clients = atoi (optarg);
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
		case 'x':
			operator = optarg;
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

	n_clients = read_client_number (fs, argv[0]);

	control = init_control (fs, argv[0], argv[1], n_clients);
	assert (control);

	if (mute) {
		control->fp_output = fopen("/dev/null", "w");
		assert(control->fp_output);
	}
	else
		control->fp_output = stdout;

	control->slice_count = slice_count;
	control->show_latency = show_latency;
	control->direct = direct;

	if (client >= 0) {
		sprintf (qbuf, ".client none\n");
		ixsql_control_cmd (control, qbuf);

		sprintf (qbuf, ".client %d\n", client);
		ixsql_control_cmd (control, qbuf);
	}

	if (n_test_clients) {
		int i = 0;

		sprintf (qbuf, ".client none\n");
		ixsql_control_cmd (control, qbuf);

		for (i = 0; i < n_test_clients; i++) {
			sprintf (qbuf, ".client %d\n", i);
			ixsql_control_cmd (control, qbuf);
		}
	}

	if (benchmark) {
		signal (SIGALRM, sig_handler);

		assert (0 == alarm (duration));

		gettimeofday (&before, NULL);

		while (!terminate) {
			process_sql (sql);
			count++;
		}

		gettimeofday (&after, NULL);
		timeval_latency (&latency, &before, &after);
		elapsed_sec = timeval_to_sec (&latency);

		goto out;
	}
	else if (direct) {
		count = 1;

		gettimeofday (&before, NULL);

		if (operator)
			process_exec (sql);
		else if (sql_file)
			count = process_batch (sql_file);
		else
			process_sql (sql);

		gettimeofday (&after, NULL);
		timeval_latency (&latency, &before, &after);
		elapsed_sec = timeval_to_sec (&latency);
	}
	else
		ixsql_shell ();

out:
	if (direct && show_latency)
		fprintf (stdout,
			 "## %llu queries were processed in "
			 "%.3f sec (%.3lf ops/sec)\n",
			 _llu (count),
			 elapsed_sec, 1.0 * count / elapsed_sec);

        glfs_fini (fs);

	if (control)
		free (control);

        return ret;
}
