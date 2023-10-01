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

struct gsh_parsed {
	/* List of tokens from previous input line. */
	const char **tokens;

	/* Any dynamically allocated tokens. */
	const char **alloc;
	size_t alloc_n;
};

extern char **environ;

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

/*      Substitute a parameter with its value.
 *
 *      If an allocation was performed, returns the address of the buffer.
 *      Otherwise, returns NULL.
 */
static const char *gsh_fmt_param(struct gsh_params *params,
				 const char **const var)
{
	char *subst_buf;

	switch ((*var)[1]) {
	case '?':
		asprintf(&subst_buf, "%d", params->last_status);

		return (*var = subst_buf);
	default: // Non-special.
		*var = envz_get(*environ, params->env_len, &(*var)[1]);
		return NULL;
	}
}

/*      Expand tokens.
 *
 *      If an allocation was performed, returns the address of the buffer.
 *      Otherwise, returns NULL.
 */
static const char *gsh_parse_tok(struct gsh_params *params,
				 const char **const tok)
{
	char *subst_buf;

	// TODO: globbing, piping
	switch ((*tok)[0]) {
	case '$':
		return gsh_fmt_param(params, tok);
	case '~':
		subst_buf = malloc(strlen(*tok) + params->home_len + 1);
		strcpy(stpcpy(subst_buf, params->homevar), *tok + 1);

		return (*tok = subst_buf);
	default:
		return NULL;
	}
}

/*      Retrieve the filename within the pathname, outputting both.
 *
 *       If an allocation was performed, returns the address of the buffer.
 *       Otherwise, returns NULL.
 */
static const char *gsh_parse_filename(struct gsh_params *params,
				      const char **const out_pathname,
				      const char **const filename)
{
	// Perform any substitutions.
	const char *tok_buf = gsh_parse_tok(params, filename);

	// Get the pathname, whether relative or absolute, if one
	// preceded the filename.
	char *last_slash = strrchr(*filename, '/');

	if (last_slash) {
		*out_pathname = *filename;
		*filename = last_slash + 1;
	} else {
		*out_pathname = NULL;
	}

	return tok_buf;
}

/*	Return argument list terminated with NULL, and pathname.
 *	A NULL pathname means the PATH environment variable must be used.
 */
static void gsh_parse_line(struct gsh_params *params, struct gsh_parsed *parsed,
			   const char **const out_pathname, char *const line)
{
	parsed->tokens[0] = strtok(line, " \n");

	const char *allocated;

	if ((allocated = gsh_parse_filename(params, out_pathname,
					    &parsed->tokens[0])))
		parsed->alloc[parsed->alloc_n++] = allocated;

	// Get arguments.
	for (int arg_n = 1; (parsed->tokens[arg_n] = strtok(NULL, " \n")) &&
			    arg_n <= GSH_MAX_ARGS;
	     ++arg_n) {
		if ((allocated = gsh_parse_tok(params, &parsed->tokens[arg_n])))
			parsed->alloc[parsed->alloc_n++] = allocated;
	}
}

static void gsh_free_parsed(struct gsh_parsed *parsed)
{
	// Delete any token substitution buffers.
	while (parsed->alloc_n > 0)
		free((char *)parsed->alloc[parsed->alloc_n--]);
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

static void gsh_init_env(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->pathvar = envz_get(*environ, params->env_len, "PATH");

	params->homevar = envz_get(*environ, params->env_len, "HOME");
	params->home_len = strlen(params->homevar);
}

void gsh_init(struct gsh_state *sh)
{
	gsh_init_env(&sh->params);
	gsh_init_wd((sh->wd = malloc(sizeof(*sh->wd))));

	sh->parsed = malloc(sizeof(*sh->parsed));
	sh->parsed->tokens = malloc(sizeof(char *) * GSH_MAX_ARGS);
	sh->parsed->alloc = malloc(sizeof(char *) * GSH_MAX_ARGS);
	sh->parsed->alloc_n = 0;

	sh->hist = malloc(sizeof(*sh->hist));
	sh->hist->cmd_history = sh->hist->oldest_cmd = NULL;
	sh->hist->hist_n = 0;

	sh->params.last_status = 0;

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

ssize_t gsh_read_line(const struct gsh_state *sh, char **const out_line)
{
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
			 const char **args)
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
static int gsh_exec(struct gsh_state *sh, const char *pathname,
		    const char **args)
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

static int gsh_switch(struct gsh_state *sh, const char *pathname,
		      const char **args)
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

void gsh_run_cmd(struct gsh_state *sh, size_t len, char *line)
{
	if (len == 0)
		return;

	gsh_add_hist(sh->hist, len, line);

	const char *pathname;
	gsh_parse_line(&sh->params, sh->parsed, &pathname, line);

	sh->params.last_status = gsh_switch(sh, pathname, sh->parsed->tokens);

	gsh_free_parsed(sh->parsed);
}
