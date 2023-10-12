#define _GNU_SOURCE // for reentrant hashtables
#include <search.h>

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

#include "gsh.h"
#include "parse.h"
#include "history.h"
#include "builtin.h"
#include "process.h"

#define GSH_PROMPT(cwd) "\033[46m" cwd "\033[49m@ "

#ifndef NDEBUG
bool g_gsh_initialized = false;
#endif

extern char **environ;

void gsh_put_prompt(const struct gsh_state *sh)
{
	if (sh->shopts.prompt_status)
		printf("<%d> ", WIFEXITED(sh->params.last_status) ?
					WEXITSTATUS(sh->params.last_status) :
					255);

	const bool in_home = strncmp(sh->wd->cwd,
				     gsh_getenv(&sh->params, "HOME"),
				     sh->params.home_len) == 0;

	printf((in_home ? GSH_PROMPT("~%s") : GSH_PROMPT("%s")),
	       (in_home ? sh->params.home_len : 0) + sh->wd->cwd);
}

void gsh_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n", (msg ? ":" : ""),
	       (msg ? msg : ""), (err ? "(" : ""), (err ? strerror(err) : ""),
	       (err ? ")" : ""));
}

char *gsh_getenv(const struct gsh_params *params, const char *name)
{
	assert(params->env_len > 0);

	char *const value = envz_get(*environ, params->env_len, name);
	return (value ? value : "");
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

static struct gsh_workdir *gsh_init_wd()
{
	struct gsh_workdir *wd = malloc(sizeof(*wd));

	// Get working dir and its max path length.
	wd->cwd = malloc((size_t)(wd->max_path = _POSIX_PATH_MAX));
	gsh_getcwd(wd);

	// Get maximum length of terminal input line.
	wd->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	return wd;
}

/*	The maximum length of an input line on the terminal
 *	that will currently be accepted, not including the newline
 *	or null byte.
 */
size_t gsh_max_input(const struct gsh_state *sh)
{
	return (size_t)sh->wd->max_input - sh->input_len;
}

static void gsh_set_params(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->home_len = strlen(gsh_getenv(params, "HOME"));

	params->last_status = 0;
}

void gsh_init(struct gsh_state *sh)
{
	gsh_set_builtins(&sh->builtin_tbl);
	gsh_set_params(&sh->params);

	sh->wd = gsh_init_wd();
	sh->parsed = gsh_init_parsed();
	sh->hist = gsh_init_hist();

	sh->input_len = 0;

	// Max input line length + newline + null byte.
	sh->line = malloc(gsh_max_input(sh) + 2);

	sh->shopts = (struct gsh_shopts){ 0 };

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

/* Copy a null-terminated path from PATH variable, stopping when a colon ':' or
 * null terminator is encountered. */
static void gsh_copy_pathname(char **const dest_it, const char **const src_it)
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

// TODO: (!) Replace with exec(3).
// TODO: (!) Move wd->max_path to parse state?
static int gsh_exec_path(const char *pathvar, const struct gsh_workdir *wd,
			 char *const *args)
{
	char *const exec_buf = malloc((size_t)wd->max_path);
	char *exec_pathname;

	for (const char *path_it = pathvar; *path_it;) {
		exec_pathname = exec_buf;

		gsh_copy_pathname(&exec_pathname, &path_it);
		sprintf(exec_pathname, "/%s", args[0]);

		execve(exec_buf, args, environ);
	}

	free(exec_buf);

	return -1;
}

/* Fork and exec a program. */
static int gsh_exec(struct gsh_state *sh, char *pathname, char *const *args)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, &sh->params.last_status, 0);
		return sh->params.last_status;
	}

	if (pathname)
		execve(pathname, args, environ);
	else
		gsh_exec_path(gsh_getenv(&sh->params, "PATH"), sh->wd, args);

	// Named program couldn't be executed.
	gsh_bad_cmd(sh->line, errno);
	exit(GSH_EXIT_NOTFOUND);
}

int gsh_switch(struct gsh_state *sh, char *pathname, char *const *args)
{
	if (strcmp(args[0], "exit") == 0)
		exit(EXIT_SUCCESS);

	ENTRY *builtin;
	if (hsearch_r((ENTRY){ .key = args[0] }, FIND, &builtin,
		      sh->builtin_tbl))
		return GSH_BUILTIN_FUNC(builtin)(sh, args);
	else
		return gsh_exec(sh, pathname, args);
}

void gsh_run_cmd(struct gsh_state *sh)
{
	assert(g_gsh_initialized);

	while (gsh_read_line(sh))
		;

	if (sh->line[0] == '\0')
		return;

	gsh_add_hist(sh->hist, sh->input_len, sh->line);
	gsh_parse_and_run(sh);
}
