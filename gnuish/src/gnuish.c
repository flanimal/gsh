#include <unistd.h>
#include <limits.h>
#include <envz.h>
#include <sys/wait.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "gnuish.h"
#include "parse.h"
#include "builtin.h"
#include "history.h"
#include "process.h"

#define GSH_PROMPT(cwd) "\033[46m" cwd "\033[49m@ "
#define GSH_SECOND_PROMPT "> "

#ifndef NDEBUG
static bool g_gsh_initialized = false;
#endif

static void gsh_put_prompt(const struct gsh_params *params, const char *cwd)
{
	const bool in_home = strncmp(cwd, params->homevar, params->home_len) ==
	    0;

	const int status = WIFEXITED(params->last_status) ?
	    WEXITSTATUS(params->last_status) : 255;

	printf((in_home ? "<%d>" GSH_PROMPT("~%s") : "<%d>" GSH_PROMPT("%s")),
	       status, cwd + (in_home ? params->home_len : 0));
}

void gsh_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n", (msg ? ":" : ""),
	       (msg ? msg : ""), (err ? "(" : ""), (err ? strerror(err) : ""),
	       (err ? ")" : ""));
}

void gsh_getcwd(struct gsh_workdir *wd)
{
	if (getcwd(wd->cwd, (size_t)wd->max_path))
		return;

	/* Current working path longer than max_path chars. */
	free(wd->cwd);

	// We will use the buffer allocated by `getcwd`
	// to store the working directory from now on.
	wd->cwd = getcwd(NULL, 0);
	wd->max_path = pathconf(wd->cwd, _PC_PATH_MAX);
}

static void gsh_init_wd(struct gsh_workdir *wd)
{
	// Get working dir and its max path length.
	wd->cwd = malloc((size_t)(wd->max_path = _POSIX_PATH_MAX));
	gsh_getcwd(wd);

	// Get maximum length of terminal input line.
	wd->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);
}

/*	The maximum length of an input line on the terminal
 *	that will currently be accepted, not including the newline
 *	or null byte.
 */
static size_t gsh_max_input(const struct gsh_state *sh)
{
	return (size_t)sh->wd->max_input - sh->input_len;
}

static void gsh_init_params(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->pathvar = envz_get(*environ, params->env_len, "PATH");

	params->homevar = envz_get(*environ, params->env_len, "HOME");
	params->home_len = strlen(params->homevar);

	params->last_status = 0;
}

void gsh_init(struct gsh_state *sh)
{
	gsh_init_params(&sh->params);
	gsh_init_wd((sh->wd = malloc(sizeof(*sh->wd))));
	gsh_init_parsed((sh->parsed = malloc(sizeof(*sh->parsed))));

	sh->hist = malloc(sizeof(*sh->hist));
	sh->hist->cmd_history = sh->hist->oldest_cmd = NULL;
	sh->hist->hist_n = 0;

	sh->line_it = sh->line = malloc((size_t)sh->wd->max_input);
	sh->input_len = 0;

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

bool gsh_read_line(struct gsh_state *sh)
{
	// Check if called to get more input.
	if (*sh->parsed->token_it) {
		sh->line_it += sh->input_len;

		fputs(GSH_SECOND_PROMPT, stdout);
	} else {
		sh->line_it = sh->line;
		sh->input_len = 0;

		gsh_put_prompt(&sh->params, sh->wd->cwd);
	}

	// FIXME: Correctly handle if getline resizes the buffer.
	size_t max_input = gsh_max_input(sh) - sh->input_len;
	ssize_t len = getline(&sh->line_it, &max_input, stdin);

	if (len == -1)
		return false;

	sh->line_it[len - 1] = '\0';	// Remove newline.
	sh->input_len += (size_t)(len - 1);

	return true;
}

/* Copy a null-terminated path from PATH variable, stopping when a colon ':' or
 * null terminator is encountered. */
static void copy_path_ent(char **const dest_it, const char **const src_it)
{
	for (;; ++(*dest_it), ++(*src_it)) {
		switch (**src_it) {
		case ':':
			++(*src_it);	// Move to next path following the colon.
			/* fall through */
		case '\0':
			**dest_it = '\0';
			return;
		default:
			**dest_it = **src_it;
			continue;
		}
	}
}

static int gsh_exec_path(const char *pathvar, const struct gsh_workdir *wd,
			 char **args)
{
	char *const exec_buf = malloc((size_t)wd->max_path);
	char *exec_pathname;

	for (const char *path_it = pathvar; *path_it;) {
		exec_pathname = exec_buf;

		copy_path_ent(&exec_pathname, &path_it);
		sprintf(exec_pathname, "/%s", args[0]);

		execve(exec_buf, (char *const *)args, environ);
	}

	free(exec_buf);

	return -1;
}

/* Fork and exec a program. */
static int gsh_exec(struct gsh_state *sh, char **args)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, &sh->params.last_status, 0);
		return sh->params.last_status;
	}

	if (sh->parsed->has_pathname)
		execve(sh->line, (char *const *)args, environ);
	else
		gsh_exec_path(sh->params.pathvar, sh->wd, args);

	// Named program couldn't be executed.
	gsh_bad_cmd(sh->line, errno);
	exit(GSH_EXIT_NOTFOUND);
}

/* Re-run the n-th previous line of input. */
static int gsh_recall(struct gsh_state *sh, const char *recall_arg)
{
	int n_arg = (recall_arg ? atoi(recall_arg) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gsh_bad_cmd("no matching history entry", 0);
		return -1;
	}

	struct gsh_hist_ent *cmd_it = sh->hist->cmd_history;

	while (cmd_it->forw && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);

	// Ensure that parse state from recall invocation is not
	// reused.
	gsh_free_parsed(sh->parsed);

	// Make a copy so we don't lose it if the history entry
	// gets deleted.
	sh->line_it = strcpy(sh->line, cmd_it->line);
	sh->input_len = cmd_it->len;

	gsh_run_cmd(sh);

	return sh->params.last_status;
}

static int gsh_switch(struct gsh_state *sh)
{
	char **args = sh->parsed->tokens;

	// TODO: hash table or something
	if (strcmp(args[0], "cd") == 0)
		return gsh_chdir(sh->wd, args[1]);

	else if (strcmp(args[0], "r") == 0)
		return gsh_recall(sh, args[1]);

	else if (strcmp(args[0], "hist") == 0)
		return gsh_list_hist(sh->hist->cmd_history);

	else if (strcmp(args[0], "echo") == 0)
		return gsh_echo(args + 1);

	else if (strcmp(args[0], "help") == 0)
		return gsh_puthelp();

	else if (strcmp(args[0], "exit") == 0)
		exit(EXIT_SUCCESS);

	else
		return gsh_exec(sh, args);
}

void gsh_run_cmd(struct gsh_state *sh)
{
	if (sh->input_len == 0)
		return;

	gsh_add_hist(sh->hist, sh->input_len, sh->line);

	while (gsh_parse_filename(&sh->params, sh->parsed, sh->line) ||
	       gsh_parse_args(&sh->params, sh->parsed)) {
		if (!gsh_read_line(sh))
			return;	// Error occurred when getting more input.
	}

	sh->params.last_status = gsh_switch(sh);

	gsh_free_parsed(sh->parsed);
}
