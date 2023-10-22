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
		printf(GSH_PROMPT);
		return;
	}

	const bool in_home = strncmp(sh->cwd,
				     gsh_getenv(&sh->params, "HOME"),
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
	if (getcwd(sh->cwd, (size_t)sh->input->max_path))
		return;

	/* Current working path longer than max_path chars. */
	free(sh->cwd);

	// We will use the buffer allocated by `getcwd`
	// to store the working directory from now on.
	sh->cwd = getcwd(NULL, 0);
	sh->input->max_path = pathconf(sh->cwd, _PC_PATH_MAX);
}

/*	The maximum length of an input line on the terminal
 *	that will currently be accepted, not including the newline
 *	or null byte.
 */
size_t gsh_max_input(const struct gsh_state *sh)
{
	return (size_t)sh->input->max_input;
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

static struct gsh_input_buf *gsh_init_inputbuf()
{
	struct gsh_input_buf *input = malloc(sizeof(*input));
	// Get maximum length of terminal input line.
	input->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	// Max input line length + newline + null byte.
	input->line = malloc((size_t)input->max_input + 2);

	return input;
}

void gsh_init(struct gsh_state *sh)
{
	gsh_set_builtins(&sh->builtin_tbl);
	gsh_set_shopts(&sh->shopt_tbl);
	gsh_set_params(&sh->params);

	// Get working dir and its max path length.
	sh->cwd = malloc((size_t)(sh->input->max_path = _POSIX_PATH_MAX));
	gsh_getcwd(sh);

	sh->input = gsh_init_inputbuf();
	sh->hist = gsh_init_hist();

	sh->shopts = GSH_OPT_DEFAULTS;

	sh->parse_bufs = gsh_init_parsebufs();
	sh->parse_state = NULL;

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

	// TODO: Move this outside of run_cmd()
	// so that we can also use run_cmd() for recalls?
	while (gsh_read_line(sh->input))
		;

	if (strcspn(sh->input->line, " ") == 0)
		return;

	gsh_add_hist(sh->hist, sh->input->input_len, sh->input->line);
	gsh_parse_and_run(sh);

	sh->input->input_len = 0;
}
