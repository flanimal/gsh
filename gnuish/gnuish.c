#include <unistd.h>
#include <limits.h>
#include <search.h>
#include <envz.h>
#include <sys/wait.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "gnuish.h"

#define GNUISH_PROMPT "@"
#define GNUISH_MAX_ARGS 64

static void gnuish_put_prompt(const struct gnuish_state *sh_state)
{
	printf("%s %s ", sh_state->cwd, GNUISH_PROMPT);
}

/*	Returns argument list terminated with NULL, and pathname.
 *	A NULL pathname means the PATH environment variable must be used.
 */
static void gnuish_parse_line(char *line, const char **out_pathname, char **out_args)
{
	*out_pathname = NULL;
	out_args[0] = strtok(line, " \n");

	if (**out_args == '/') {
		*out_pathname = out_args[0];
		out_args[0] = strrchr(out_args[0], '/') + 1;
	}

	for (int arg_n = 1; (out_args[arg_n] = strtok(NULL, " \n")) &&
			    arg_n <= GNUISH_MAX_ARGS;
	     ++arg_n)
		;
}

static void gnuish_add_hist(struct gnuish_state *sh_state, size_t len,
			    const char *line)
{
	struct gnuish_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_state->cmd_history);
	sh_state->cmd_history = last_cmd;

	// Take care to include null character.
	strcpy((last_cmd->line = malloc(len)), line);
	last_cmd->len = len;

	if (sh_state->hist_n == 10) {
		struct gnuish_hist_ent *popped_ent = sh_state->oldest_cmd;
		sh_state->oldest_cmd = popped_ent->back;

		remque(popped_ent);
		free(popped_ent);

		return;
	}

	if (sh_state->hist_n == 0)
		sh_state->oldest_cmd = last_cmd;

	++sh_state->hist_n;
}

static void gnuish_list_hist(const struct gnuish_state *sh_state)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;
	struct gnuish_hist_ent *cmd_it = sh_state->cmd_history;

	for (; cmd_it; cmd_it = cmd_it->forw)
		printf("%i: %s\n", cmd_n++, cmd_it->line);
}

static void gnuish_bad_cmd(struct gnuish_state *sh_state, int err)
{
	if (err)
		printf("not a command: %m\n", err);
	else
		printf("not a command\n");
}

/* Re-run the n-th previous line of input. */
static void gnuish_recall(struct gnuish_state *sh_state)
{
	struct gnuish_hist_ent *cmd_it = sh_state->cmd_history;
	
	int n_arg = (sh_state->args[1] ? atoi(sh_state->args[1]) : 1);

	if (0 >= n_arg || sh_state->hist_n < n_arg) {
		gnuish_bad_cmd(sh_state, 0);
		return;	
	}

	while (cmd_it && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);
	gnuish_run_cmd(sh_state, cmd_it->len, cmd_it->line);
}

static void gnuish_getcwd(struct gnuish_state *sh_state)
{
	if (!getcwd(sh_state->cwd, (size_t)sh_state->max_path)) {
		/* Current working path longer than max_path chars. */
		free(sh_state->cwd);

		// We will use the buffer allocated by `getcwd`
		// to store the working directory from now on.
		sh_state->cwd = getcwd(NULL, 0);
		sh_state->max_path = pathconf(sh_state->cwd, _PC_PATH_MAX);
	}
}

void gnuish_init(struct gnuish_state *sh_state, char *const *envp)
{
	sh_state->env = envp;

	size_t env_len = 0;
	for (; *envp; ++envp)
		env_len += strlen(*envp);

	sh_state->path = envz_get(*sh_state->env, env_len, "PATH");

	// Get working dir and its max path length.
	sh_state->cwd = malloc((size_t)(sh_state->max_path = _POSIX_PATH_MAX));
	gnuish_getcwd(sh_state);

	// Get maximum length of terminal input line.
	sh_state->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	sh_state->cmd_history = sh_state->oldest_cmd = NULL;
	sh_state->hist_n = 0;

	sh_state->args = malloc(sizeof(char *) * GNUISH_MAX_ARGS);
}

size_t gnuish_read_line(struct gnuish_state *sh_state, char *out_line)
{
	gnuish_put_prompt(sh_state);

	ssize_t len = getline(&out_line, &sh_state->max_input, stdin);

	if (len == -1) {
		printf("%m\n", errno);
		exit(EXIT_FAILURE);
	}

	out_line[len - 1] = '\0'; // Remove newline.

	return (size_t)len;
}

void gnuish_run_cmd(struct gnuish_state *sh_state, size_t len, char *line)
{
	const char *pathname;

	if (!(line[0] == 'r' &&
	      (line[1] == '\0' || isspace(line[1])))) // Recall `r` should NOT
						      // be added to history.
		gnuish_add_hist(sh_state, len, line);

	gnuish_parse_line(line, &pathname, sh_state->args);
	const char *const filename = sh_state->args[0];

	// TODO:
	if (strcmp(filename, "cd") == 0)

		gnuish_chdir(sh_state);

	else if (strcmp(filename, "r") == 0)

		gnuish_recall(sh_state);

	else if (strcmp(filename, "exit") == 0)

		exit(EXIT_SUCCESS);

	else if (strcmp(filename, "hist") == 0)

		gnuish_list_hist(sh_state);

	else if (strcmp(filename, "echo") == 0)

		gnuish_echo(sh_state);

	else

		gnuish_exec(sh_state, pathname);
}

static int gnuish_exec_path(struct gnuish_state *sh_state)
{
	int code = -1;

		char *path = malloc(strlen(sh_state->path) + 1);
		strcpy(path, sh_state->path);

		char *exec_pathname = malloc((size_t)sh_state->max_path);

		for (char *path_it = strtok(path, ":"); code == -1 && path_it;
		     path_it = strtok(NULL, ":")) {
		sprintf(exec_pathname, "%s/%s", path_it, sh_state->args[0]);

		code = execve(exec_pathname, sh_state->args, sh_state->env);
	}

	return code;
}

void gnuish_exec(struct gnuish_state *sh_state, char *pathname)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, NULL, 0);
		return;
	}

	if (-1 == (pathname ? execve(pathname, sh_state->args, sh_state->env) :
			      gnuish_exec_path(sh_state)))
		gnuish_bad_cmd(sh_state, errno);
}

void gnuish_echo(struct gnuish_state *sh_state)
{
	char *const *args = sh_state->args;

	// Increment at start to skip name of builtin.
	for (++args; *args; ++args)
		printf("%s ", *args);

	putchar('\n');
}

void gnuish_chdir(struct gnuish_state *sh_state)
{
	const char *const pathname = sh_state->args[1];

	if (chdir(pathname) == -1)
		printf("%m\n", errno);

	gnuish_getcwd(sh_state);
}