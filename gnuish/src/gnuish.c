#define _GNU_SOURCE

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
#include "builtin.h"
#include "history.h"
#include "system.h"

/* The maximum number of arguments that can be passed on the command line. */
#define GSH_MAX_ARGS 64

#define GSH_EXIT_NOTFOUND 127

#define GSH_PROMPT(cwd) "\033[46m" cwd "\033[49m@ "
#define GSH_SECOND_PROMPT "> "

extern char **environ;

struct gsh_parsed {
	bool has_pathname;

	/* List of tokens from previous input line. */
	char **tokens, **token_it;
	size_t token_n;

	/* Any dynamically allocated tokens. */
	char **alloc;
};

#ifndef NDEBUG
static bool g_gsh_initialized = false;
#endif

static void gsh_put_prompt(const struct gsh_params *params, const char *cwd)
{
	const bool in_home = strncmp(cwd, params->homevar, params->home_len) ==
			     0;

	const int status = WIFEXITED(params->last_status) ?
				   WEXITSTATUS(params->last_status) :
				   255;

	printf((in_home ? "<%d>" GSH_PROMPT("~%s") : "<%d>" GSH_PROMPT("%s")),
	       status, cwd + (in_home ? params->home_len : 0));
}

void gsh_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n", (msg ? ":" : ""),
	       (msg ? msg : ""), (err ? "(" : ""), (err ? strerror(err) : ""),
	       (err ? ")" : ""));
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed)
{
	switch (parsed->token_it[-1][1]) {
	case '?':
		asprintf(parsed->alloc, "%d", params->last_status);

		parsed->token_it[-1] = *parsed->alloc++;
		break;
	default: // Non-special.
		parsed->token_it[-1] = envz_get(*environ, params->env_len,
						&parsed->token_it[-1][2]);
		break;
	}
}

/*      Expand the last token.
 */
static void gsh_expand_tok(struct gsh_params *params, struct gsh_parsed *parsed)
{
	// TODO: globbing, piping
	switch (parsed->token_it[-1][0]) {
	case '$':
		gsh_fmt_param(params, parsed);
		break;
	case '~':
		*parsed->alloc = malloc(strlen(parsed->token_it[-1]) +
					params->home_len + 1);

		strcpy(stpcpy(*parsed->alloc, params->homevar),
		       &parsed->token_it[-1][1]);

		parsed->token_it[-1] = *parsed->alloc++;
		break;
	}
}

// TODO: strtok_r

/*      Returns true while there are still more tokens to collect,
 *	similar to strtok.
 *
 *      By default, a backslash \ is the _line continuation character_.
 *
 *      When it is the last character in an input line, it invokes a
 *      secondary prompt for more input, which will be concatenated to the first
 *      line, and the backslash \ will be excluded.
 *
 *      Or, in other words, it concatenates what appears on both sides of it,
 *      skipping null bytes and newlines, but stopping at spaces.
 *
 *      Or, in other *other* words, it means to append to the preceding token,
 *      stopping at spaces.
 */
static bool gsh_next_tok(struct gsh_parsed *parsed, char *const line)
{
	char *const next_tok =
		strtok((*parsed->token_it ? *parsed->token_it : line), " ");

	if (!next_tok)
		// Reached the null byte, meaning there weren't any
		// continuations. No more tokens available or needed.
		return false;

	char *line_cont = strchr(next_tok, '\\');

	if (!line_cont) {
		// No line continuation, and more input available.
		// Get more tokens.
		*parsed->token_it++ = next_tok;
		++parsed->token_n;
	} else if (line_cont[1] == '\0') {
		// Line continuation followed by null byte.
		// Need more input to finish the current token.
		*parsed->token_it = next_tok;
	} else {
		// Line continuation followed by more input.
		for (; *line_cont; ++line_cont)
			line_cont[0] = line_cont[1];

		*parsed->token_it++ = next_tok;
		++parsed->token_n;
	}

	return true;
}

/*      Parse the first token in the input line, and place
 *      the filename in the argument array.
 */
static bool gsh_parse_filename(struct gsh_params *params,
			       struct gsh_parsed *parsed, char *line)
{
	if (gsh_next_tok(parsed, line) && *parsed->token_it)
		return true;

	// Perform any substitutions.
	gsh_expand_tok(params, parsed);

	char *last_slash = strrchr(line, '/');
	if (last_slash)
		*parsed->token_it = last_slash + 1;

	parsed->has_pathname = !!last_slash;
	return false;
}

/*	Parse tokens and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 *
 *      Returns true if we need more input, false if we're done.
 */
static bool gsh_parse_args(struct gsh_params *params, struct gsh_parsed *parsed)
{
	// Get arguments.
	while (parsed->token_n <= GSH_MAX_ARGS && gsh_next_tok(parsed, NULL)) {
		// If gsh_next_tok() returned true but we're still
		// on an incomplete token, then we need more input.
		if (*parsed->token_it)
			return true;

		gsh_expand_tok(params, parsed);
	}

	// We've gotten all the input we need to parse a line.
	return false;
}

void gsh_free_parsed(struct gsh_parsed *parsed)
{
	// Delete any token substitution buffers.
	while (parsed->alloc[-1])
		free(*parsed->alloc--);

	// Mark tokens as empty.
	for (; parsed->token_n > 0; --parsed->token_n)
		*(--parsed->token_it) = NULL;
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

size_t gsh_max_input(const struct gsh_state *sh)
{
	assert(g_gsh_initialized);
	return (size_t)sh->wd->max_input;
}

static void gsh_init_params(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->pathvar = envz_get(*environ, params->env_len, "PATH");

	params->homevar = envz_get(*environ, params->env_len, "HOME");
	params->home_len = strlen(params->homevar);

	params->last_status = 0; // TODO: Designated initializer?
}

static void gsh_init_parsed(struct gsh_parsed *parsed)
{
	parsed->token_it = parsed->tokens =
		calloc(GSH_MAX_ARGS, sizeof(char *));
	parsed->token_n = 0;

	parsed->alloc =
		malloc(sizeof(char *) * (GSH_MAX_ARGS + 1)); // MAX_ARGS plus
							     // sentinel.
	*parsed->alloc++ = NULL; // Set empty sentinel.
}

void gsh_init(struct gsh_state *sh)
{
	gsh_init_params(&sh->params);
	gsh_init_wd((sh->wd = malloc(sizeof(*sh->wd))));
	gsh_init_parsed((sh->parsed = malloc(sizeof(*sh->parsed))));

	sh->hist = malloc(sizeof(*sh->hist));
	sh->hist->cmd_history = sh->hist->oldest_cmd = NULL;
	sh->hist->hist_n = 0;

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

ssize_t gsh_read_line(const struct gsh_state *sh, char **const out_line)
{
	if (*sh->parsed->token_it) // Check if called to get more input.
		fputs(GSH_SECOND_PROMPT, stdout);
	else
		gsh_put_prompt(&sh->params, sh->wd->cwd);

	ssize_t len = getline(out_line, (size_t *)&sh->wd->max_input, stdin);

	(*out_line)[len - 1] = '\0'; // Remove newline.
	return len - 1;
}

/* Copy a null-terminated path from PATH variable, stopping when a colon ':' or
 * null terminator is encountered. */
static void copy_path_ent(char **const dest_it, const char **const src_it)
{
	for (;; ++(*dest_it), ++(*src_it)) {
		switch (**src_it) {
		case ':':
			++(*src_it); // Move to next path following the colon.
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

// TODO: Call the functions for path and for no path directly in run_cmd above?
// Also pass sub-structs directly.
/* Fork and exec a program. */
static int gsh_exec(struct gsh_state *sh, const char *pathname, char **args)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, &sh->params.last_status, 0);
		return sh->params.last_status;
	}

	if (pathname)
		execve(pathname, (char *const *)args, environ);
	else
		gsh_exec_path(sh->params.pathvar, sh->wd, args);

	// Named program couldn't be executed.
	gsh_bad_cmd((pathname ? pathname : args[0]), errno);
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
	char *ent_line_cpy = strcpy(malloc(cmd_it->len + 1), cmd_it->line);
	sh->parsed->input_len = cmd_it->len;

	gsh_run_cmd(sh, ent_line_cpy);
	free(ent_line_cpy);

	return sh->params.last_status;
}

static int gsh_switch(struct gsh_state *sh, const char *pathname, char **args)
{
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
		return gsh_exec(sh, pathname, args);
}

void gsh_run_cmd(struct gsh_state *sh, ssize_t len, char *line)
{
	if (len == 0)
		return;

	gsh_add_hist(sh->hist, (size_t)len, line);

	char *line_it = line;
	// TODO: combine, simply, put into subroutines or something

	while (gsh_parse_filename(&sh->params, sh->parsed, line)) {
		line_it += len;

		if ((len = gsh_read_line(sh, &line_it)) == -1)
			return; // Error reading line.
	}

	while (gsh_parse_args(&sh->params, sh->parsed)) {
		line_it += len;

		if ((len = gsh_read_line(sh, &line_it)) == -1)
			return;
	}

	sh->params.last_status =
		gsh_switch(sh, (sh->parsed->has_pathname ? line : NULL),
			   sh->parsed->tokens);

	gsh_free_parsed(sh->parsed);
}
