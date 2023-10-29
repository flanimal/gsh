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
#include <ctype.h>

#include "gsh.h"
#include "input.h"
#include "parse.h"
#include "history.h"
#include "builtin.h"
#include "process.h"

#define GSH_PROMPT "@ "
#define GSH_WORKDIR_PROMPT(cwd) "\033[46m" cwd "\033[49m" GSH_PROMPT

#ifndef NDEBUG
bool g_gsh_initialized = false;
#endif

extern char **environ;

void gsh_put_prompt(const struct gsh_state *sh)
{
	if (sh->shopts & GSH_OPT_PROMPT_STATUS)
		printf("<%d> ", (WIFEXITED(sh->params.last_status)) ?
					WEXITSTATUS(sh->params.last_status) :
					255);

	if (!(sh->shopts & GSH_OPT_PROMPT_WORKDIR)) {
		fputs(GSH_PROMPT, stdout);
		return;
	}

	const bool in_home = strncmp(sh->cwd, gsh_getenv(&sh->params, "HOME"),
				     sh->params.home_len) == 0;

	printf((in_home) ? GSH_WORKDIR_PROMPT("~%s") : GSH_WORKDIR_PROMPT("%s"),
	       &sh->cwd[(in_home) ? sh->params.home_len : 0]);
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

void gsh_getcwd(struct gsh_state *sh)
{
	if (getcwd(sh->cwd, (size_t)sh->max_path))
		return;

	/* Current working path longer than max_path chars. */
	free(sh->cwd);

	// We will use the buffer allocated by `getcwd`
	// to store the working directory from now on.
	sh->cwd = getcwd(NULL, 0);
	sh->max_path = pathconf(sh->cwd, _PC_PATH_MAX);
}

static void gsh_set_params(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->home_len = strlen(gsh_getenv(params, "HOME"));

	params->last_status = 0;
}

static void gsh_set_shopts(struct hsearch_data **shopt_tbl)
{
	struct gsh_shopt {
		char *cmd;
		enum gsh_shopt_flags flag;
	};

	static struct gsh_shopt shopts[] = {
		{ "prompt_workdir", GSH_OPT_PROMPT_WORKDIR },
		{ "prompt_status", GSH_OPT_PROMPT_STATUS },
		{ "echo", GSH_OPT_ECHO },
	};

	create_hashtable(shopts, .cmd, .flag, *shopt_tbl);
}

void gsh_init(struct gsh_state *sh)
{
	gsh_set_builtins(&sh->builtin_tbl);
	gsh_set_shopts(&sh->shopt_tbl);
	gsh_set_params(&sh->params);

	// Get working dir and its max path length.
	sh->cwd = malloc((size_t)(sh->max_path = _POSIX_PATH_MAX));
	gsh_getcwd(sh);

	sh->inputbuf = gsh_new_inputbuf();
	sh->hist = gsh_new_hist();

	sh->cmd_queue.front = NULL;
	gsh_parse_init(&sh->parser, &sh->params);

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

static void gsh_switch(struct gsh_state *sh, struct gsh_parsed_cmd *cmd)
{
	if (cmd->argc == 0)
		return;

	if (strcmp(cmd->argv[0], "exit") == 0)
		exit(EXIT_SUCCESS);

	ENTRY *builtin;
	if (hsearch_r((ENTRY){ .key = cmd->argv[0] }, FIND, &builtin,
		      sh->builtin_tbl))
		sh->params.last_status =
			GSH_BUILTIN_FUNC(builtin)(sh, cmd->argc, cmd->argv);
	else
		sh->params.last_status = gsh_exec(cmd->pathname, cmd->argv);
}

void gsh_set_opt(struct gsh_state *sh, char *name, bool value)
{
	ENTRY *result;
	if (!hsearch_r((ENTRY){ .key = name }, FIND, &result, sh->shopt_tbl))
		return;

	const enum gsh_shopt_flags flag = *(enum gsh_shopt_flags *)result->data;

	if (value)
		sh->shopts |= flag;
	else
		sh->shopts &= ~flag;
}

// NOTE: (Idea) The entire reason gsh_input_buf exists
// is for gsh_run_cmd(). Should we move run_cmd() to input.c?
void gsh_run_cmd(struct gsh_state *sh)
{
	assert(g_gsh_initialized);

	if (strcspn(sh->inputbuf->line, WHITESPACE) == 0) {
		sh->inputbuf->len = 0;
		return;
	}

	gsh_add_hist(sh->hist, sh->inputbuf->len, sh->inputbuf->line);

	gsh_split_words(sh->parse_state, sh->inputbuf->line);
	gsh_parse_cmd(sh->parse_state, sh->cmd);
	
	gsh_switch(sh, sh->cmd);

	sh->inputbuf->len = 0;
}
