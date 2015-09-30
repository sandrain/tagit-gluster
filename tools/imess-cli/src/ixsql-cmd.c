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

#include "ixsql.h"

typedef	int (*ixsql_cmd_func_t) (ixsql_control_t *ctl, int argc, char **argv);

struct _ixsql_cmd_handler {
	const char       *key;
	const char       *help;
	ixsql_cmd_func_t  func;
};

typedef struct _ixsql_cmd_handler ixsql_cmd_handler_t;

static ixsql_cmd_handler_t ixsql_handlers [];

static int ixsql_cmd_help (ixsql_control_t *ctl, int argc, char **argv)
{
	ixsql_cmd_handler_t *handler = NULL;

	fprintf (ctl->fp_output, "\navailable commands:\n");

	for (handler = ixsql_handlers; handler->key; handler++) {
		fprintf (ctl->fp_output,
			 "  %s   \t%s\n", handler->key, handler->help);
	}

	return 0;
}

static int ixsql_cmd_quit (ixsql_control_t *ctl, int argc, char **argv)
{
	return IXSQL_QUIT;
}

static int ixsql_cmd_slice (ixsql_control_t *ctl, int argc, char **argv)
{
	switch (argc) {
	case 1:
		fprintf (ctl->fp_output,
			 "current slice count = %llu\n",
			 _llu (ctl->slice_count));
		break;

	case 2:
		ctl->slice_count = strtoull (argv[1], NULL, 0);
		fprintf (ctl->fp_output,
			 "slice count is set to %llu\n",
			 _llu (ctl->slice_count));
		break;
	default:
		fprintf (ctl->fp_output,
			 "%s expects none or a single argument.\n",
			 argv[0]);
		return -1;
	}

	return 0;
}

static int ixsql_cmd_client (ixsql_control_t *ctl, int argc, char **argv)
{
	int i         = 0;
	char star     = 0;

	switch (argc)  {
	case 1:
		fprintf (ctl->fp_output, "total %d clients, sending query to ",
				ctl->num_clients);
		if (ctl->active_client < 0)
			fprintf (ctl->fp_output, "all clients:\n");
		else
			fprintf (ctl->fp_output, "client %d:\n",
					ctl->active_client);

		for (i = 0; i < ctl->num_clients; i++) {
			star = 1;
			if (ctl->active_client >= 0 && ctl->active_client != i)
				star = 0;

			fprintf (ctl->fp_output, "[%3d] %s-client-%d%s\n",
					i, ctl->volname, i,
					star ? " [*]" : "");
		}
		break;
	case 2:
		i = atoi (argv[1]);
		if (i < -1 || i > ctl->num_clients - 1)
			fprintf (ctl->fp_output, "%d is not valid.\n", i);
		else {
			ctl->active_client = i;
			fprintf (ctl->fp_output,
				 "active client is set to %d\n",
				 ctl->active_client);
		}
		break;
	default:
		fprintf (ctl->fp_output,
			 "%s expects none or a single argument.\n",
			 argv[0]);
		return -1;
	}

	return 0;
}

static ixsql_cmd_handler_t ixsql_handlers [] =
{
	{ .key	= ".help",
	  .help = "Print the help message.",
	  .func = ixsql_cmd_help,
	},
	{ .key  = ".quit",
	  .help = "Quit the program.",
	  .func = ixsql_cmd_quit,
	},
	{ .key  = ".client",
	  .help = "Print or set the clients to send queries.",
	  .func = ixsql_cmd_client,
	},
	{ .key	= ".slice",
	  .help = "Print or set the query result slice count.",
	  .func = ixsql_cmd_slice,
	},

	{ 0, 0, 0 },
};

static int
cli_cmd_input_token_count (const char *text)
{
        int          count = 0;
        const char  *trav = NULL;
        int          is_spc = 1;

        for (trav = text; *trav; trav++) {
                if (*trav == ' ') {
                        is_spc = 1;
                } else {
                        if (is_spc) {
                                count++;
                                is_spc = 0;
                        }
                }
        }

        return count;
}

static void
cli_cmd_tokens_destroy (char **tokens)
{
        char **tokenp = NULL;

        if (!tokens)
                return;

        tokenp = tokens;
        while (*tokenp) {
                free (*tokenp);
                tokenp++;
        }

        free (tokens);
}

static int
process_control_cmd (ixsql_control_t *ctl, int argc, char **argv)
{
	int ret                      = 0;
	ixsql_cmd_func_t func        = NULL;
	ixsql_cmd_handler_t *handler = NULL;

	for (handler = ixsql_handlers; handler->key; handler++) {
		if (strncmp (handler->key, argv[0], strlen (handler->key)))
			continue;
		else {
			func = handler->func;
			break;
		}
	}

	if (func)
		ret = func (ctl, argc, argv);
	else
		fprintf (ctl->fp_output, "%s is not a valid command.\n",
					 argv[0]);

	return ret;
}

/*
 * API
 */

int ixsql_sql_query (ixsql_control_t *ctl, ixsql_query_t *query)
{
        int ret               = 0;
	glfs_t *fs            = NULL;
        dict_t *cmd           = NULL;
        dict_t *result        = NULL;
        struct timeval before = { 0, };
        struct timeval after  = { 0, };
	char buf[128]         = { 0, };

	fs = ctl->gluster;

        if (!cmd) {
                cmd = dict_new ();
                if (!cmd) {
                        ret = -1;
                        goto out;
                }

		ret = dict_set_str (cmd, "type", "query");
                ret = dict_set_str (cmd, "sql", query->sql);
        }

	if (ctl->active_client == -1)
                ret = dict_set_str (cmd, "clients", "all");
	else {
		sprintf(buf, "%s-client-%d", ctl->volname, ctl->active_client);
                ret = dict_set_str (cmd, "clients", buf);
	}

        gettimeofday (&before, NULL);

        ret = glfs_ipc (fs, IMESS_IPC_OP, cmd, &result);
        if (ret)
                goto out;

        gettimeofday (&after, NULL);

	query->result = result;
	timeval_latency (&query->latency, &before, &after);

out:
	if (cmd)
		dict_destroy (cmd);

	return ret;
}

int ixsql_control_cmd (ixsql_control_t *control, const char *text)
{
        int     count = 0;
        char  **tokens = NULL;
        char  **tokenp = NULL;
        char   *token = NULL;
        char   *copy = NULL;
        char   *saveptr = NULL;
        int     i = 0;
        int     ret = -1;

        count = cli_cmd_input_token_count (text);

        tokens = calloc (count + 1, sizeof (*tokens));
        if (!tokens)
                return -1;

        copy = strdup (text);
        if (!copy)
                goto out;

        tokenp = tokens;

        for (token = strtok_r (copy, " \t\r\n", &saveptr); token;
             token = strtok_r (NULL, " \t\r\n", &saveptr)) {
                *tokenp = strdup (token);

                if (!*tokenp)
                        goto out;
                tokenp++;
                i++;

        }

        ret = process_control_cmd (control, count, tokens);
out:
        free (copy);

        if (tokens)
                cli_cmd_tokens_destroy (tokens);

        return ret;
}

