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
	if (sh->shopts & GSH_OPT_PROMPT_STATUS)
		printf("<%d> ", WIFEXITED(sh->params.last_status) ?
					WEXITSTATUS(sh->params.last_status) :
					255);

	const bool in_home = strncmp(sh->wd->cwd,
				     gsh_getenv(&sh->params, "HOME"),
				     sh->params.home_len) == 0;

	printf((in_home) ? GSH_PROMPT("~%s") : GSH_PROMPT("%s"),
	       (sh->shopts & GSH_OPT_PROMPT_WORKDIR) ?
		       ((in_home) ? sh->params.home_len : 0) + sh->wd->cwd :
		       "");
}

void gsh_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n", (msg ? ":" : ""),
	       (msg ? msg : ""), (err ? "(" : ""), (err ? strerror(err) : ""),
	       (err ? ")" : ""));
}

const char *gsh_getenv(const struct gsh_params *params, const char *name)
{
	assert(params->env_len > 0);

	const char *value = envz_get(*environ, params->env_len, name);
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
	return (size_t)sh->wd->max_input;
}

static void gsh_set_params(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->home_len = strlen(gsh_getenv(params, "HOME"));

	params->last_status = 0;
}

struct gsh_shopt {
	char *cmd;
	enum gsh_shopt_flags flag;
};

static void gsh_set_shopts(struct hsearch_data **shopt_tbl)
{
	static struct gsh_shopt shopts[] = {
		{ "prompt_workdir", GSH_OPT_PROMPT_WORKDIR },
		{ "prompt_status", GSH_OPT_PROMPT_STATUS },
		{ "echo", GSH_OPT_ECHO },
	};

	const size_t shopt_n = sizeof(shopts) / sizeof(*shopts);
	// TODO: Hashtable creation function.
	hcreate_r(shopt_n, (*shopt_tbl = calloc(1, sizeof(**shopt_tbl))));

	ENTRY *result;
	for (size_t i = 0; i < shopt_n; ++i)
		hsearch_r((ENTRY){ .key = shopts[i].cmd,
				   .data = &shopts[i].flag },
			  ENTER, &result, *shopt_tbl);
}

void gsh_init(struct gsh_state *sh)
{
	gsh_set_builtins(&sh->builtin_tbl);
	gsh_set_shopts(&sh->shopt_tbl);
	gsh_set_params(&sh->params);

	sh->wd = gsh_init_wd();
	sh->parsed = gsh_init_parsed();
	sh->hist = gsh_init_hist();

	// Max input line length + newline + null byte.
	sh->line = malloc(gsh_max_input(sh) + 2);

	sh->shopts = GSH_OPT_DEFAULTS;

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

/* Fork and exec a program. */
static int gsh_exec(char *pathname, char *const *args)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		int status;
		waitpid(cmd_pid, &status, 0);

		return status;
	}

	execvp(pathname, args);

	// Named program couldn't be executed.
	gsh_bad_cmd(pathname, errno);
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
		return gsh_exec(pathname, args);
}

void gsh_run_cmd(struct gsh_state *sh)
{
	assert(g_gsh_initialized);

	size_t input_len = 0;
	while (gsh_read_line(sh, &input_len))
		;

	// NOTE: Don't run the command if it's just whitespace, either.
	// Otherwise no pathname will be found, and a NULL pointer will
	// eventually be deref'd.
	if (sh->line[strcspn(sh->line, " ")] == '\0')
		return;

	gsh_add_hist(sh->hist, input_len, sh->line);
	gsh_parse_and_run(sh);
}
