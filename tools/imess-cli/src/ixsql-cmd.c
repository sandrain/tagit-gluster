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

static inline void set_clients (ixsql_control_t *ctl, int cli)
{
	int i = 0;
	int n_active = 0;

	if (cli == -1) {
		for (i = 0; i < ctl->num_clients; i++)
			ctl->cli_mask[i] = 1;

		ctl->active_clients = ctl->num_clients;
		return;
	}
	else if (cli == -2) {
		for (i = 0; i < ctl->num_clients; i++)
			ctl->cli_mask[i] = 0;

		ctl->active_clients = 0;
		return;
	}

	ctl->cli_mask[cli] = ctl->cli_mask[cli] ? 0 : 1;

	for (i = 0; i < ctl->num_clients; i++)
		if (ctl->cli_mask[i])
			n_active++;

	ctl->active_clients = n_active;
}

static int ixsql_cmd_client (ixsql_control_t *ctl, int argc, char **argv)
{
	int i         = 0;

	switch (argc)  {
	case 1:
		fprintf (ctl->fp_output,
			 "total %d clients, sending query to %d clients:\n",
			 ctl->num_clients, ctl->active_clients);

		for (i = 0; i < ctl->num_clients; i++)
			fprintf (ctl->fp_output, "[%3d] %s-client-%d%s\n",
				 i, ctl->volname, i,
				 ctl->cli_mask[i] ? " [*]" : "");
		break;
	case 2:
		if (!strcmp ("all", argv[1]))
			set_clients (ctl, -1);
		else if (!strcmp ("none", argv[1]))
			set_clients (ctl, -2);
		else {
			char *delim = ",";
			char *sptr = NULL;
			char *token = NULL;

			token = strtok_r (argv[1], delim, &sptr);
			if (!token)
				fprintf (ctl->fp_output,
					 "%s is not understood\n", argv[1]);

			do {
				set_clients (ctl, atoi (token));
				token = strtok_r (NULL, delim, &sptr);
			} while (token);
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

	fs = ctl->gluster;

	cmd = dict_new ();
	if (!cmd) {
		ret = -1;
		goto out;
	}

	ret = dict_set_str (cmd, "type", "query");
	ret = dict_set_str (cmd, "sql", query->sql);
	if (ret)
		goto out;

	if (ctl->active_clients <= 0) {
		fprintf (ctl->fp_output, "no active clients set.\n");
		goto out;
	}

	ret = dict_set_static_bin (cmd, "cmask", ctl->cli_mask,
				   ctl->num_clients);
	if (ret)
		goto out;

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

static char *ixsql_query_type_str[] = {
	"unknown", "exec", "filter", "extractor", 0
};

int ixsql_exec (ixsql_control_t *ctl, ixsql_query_t *query)
{
        int ret               = 0;
	glfs_t *fs            = NULL;
        dict_t *cmd           = NULL;
        dict_t *result        = NULL;
        struct timeval before = { 0, };
        struct timeval after  = { 0, };
	char *typestr         = NULL;

	fs = ctl->gluster;

	cmd = dict_new ();
	if (!cmd) {
		ret = -1;
		goto out;
	}

	typestr = ixsql_query_type_str[query->type];

	ret  = dict_set_str (cmd, "type", typestr);
	ret |= dict_set_str (cmd, "sql", query->sql);
	ret |= dict_set_str (cmd, "operator", query->operator);

	/*
	ret |= dict_set_str (cmd, "clients", "all");
	if (ret) {
		fprintf (stderr, "dict_set_str failed (%d)\n", ret);
		goto out;
	}
	*/
	ret = dict_set_static_bin (cmd, "cmask", ctl->cli_mask,
			           ctl->num_clients);
	if (ret)
		goto out;

	gettimeofday (&before, NULL);

	ret = glfs_ipc (fs, IMESS_IPC_OP, cmd, &result);
	if (ret) {
		fprintf (stderr, "glfs_ipc failed (%d)\n", ret);
		goto out;
	}

	gettimeofday (&after, NULL);

	query->result = result;
	timeval_latency (&query->latency, &before, &after);

out:
	if (cmd)
		dict_destroy (cmd);

	return ret;
}

int ixsql_create_virtual_view (ixsql_control_t *ctl, char *sql, char *name)
{
	int ret = 0;
	glfs_t *fs = NULL;
	dict_t *cmd = NULL;
	dict_t *result = NULL;

	fs = ctl->gluster;

	cmd = dict_new ();
	if (!cmd) {
		ret = -1;
		goto out;
	}

	ret  = dict_set_str (cmd, "type", "meta:create-view");
	ret |= dict_set_str (cmd, "sql", sql);
	ret |= dict_set_str (cmd, "name", name);
	if (ret) {
		fprintf (stderr, "dict_set_str failed (%d)\n", ret);
		goto out;
	}

	ret = glfs_ipc (fs, IMESS_IPC_OP, cmd, &result);
	if (ret) {
		fprintf (stderr, "glfs_ipc failed (%d)\n", ret);
		goto out;
	}

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

