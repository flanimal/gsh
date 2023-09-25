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

#include "gnuish.h"

/*
 *	gnuish - GNU island shell
 *
 *	gnuish displays the current working directory in the shell prompt:
 *
 *		~ @
 *		/usr/ @
 *		/mnt/.../repos @
 *
 *	Commands
 *
 *		<command> [<args>...]	 Run command or program with optional
 *arguments.
 *
 *		r [<n>]		Execute the nth last line.
 *					The line will be placed in history, not
 *the `r` invocation. The line in question will be echoed to the screen before
 *being executed.
 *
 *	Built-ins:
 *
 *		exit		Exit the shell.
 *
 *		hist		Display up to 10 last lines entered, numbered.
 *
 *		----
 *
 *		echo		Write to stdout.
 *
 *		help		Display this help page.
 *
 *	***
 *
 *	See the "Readme" file within this directory.
 *
 *	Also see the Makefile, which:
 *		1. Compiles all source code.
 *		2. Cleans up the directory.
 *		3. Performs other tasks as required.
 */

#define GNUISH_PROMPT "@"
#define GNUISH_MAX_ARGS 64

static void gnuish_put_prompt(const struct gnuish_state *sh_state)
{
	printf("%s %s ", sh_state->cwd, GNUISH_PROMPT);
}

/*	Returns argument list terminated with NULL, and pathname.
 *	A NULL pathname means the PATH environment variable must be used.
 */
static void gnuish_parse_line(char *line, char **out_pathname, char **out_args)
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

static void gnuish_add_hist(struct gnuish_state *sh_state, const char *line)
{
	struct gnuish_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_state->cmd_history);
	sh_state->cmd_history = last_cmd;

	// Take care to include null character.
	strcpy((last_cmd->line = malloc(len)), line);
	last_cmd->len = len;

	if (sh_state->hist_n == 10) {
		struct gnuish_hist_ent *popped_ent = sh_state->oldest_cmd;
		sh_state->oldest_cmd = popped_ent->forward;

		remque(popped_ent);
		free(popped_ent);

		return;
	} else if (sh_state->hist_n == 0) {
		sh_state->oldest_cmd = last_cmd;
	}

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

/* Re-run the n-th previous line of input. */
static void gnuish_recall(struct gnuish_state *sh_state)
{
	struct gnuish_hist_ent *cmd_it = sh_state->cmd_history;
	int hist_n = (sh_state->args[1] ? atoi(sh_state->args[1]) : 1);

	while (cmd_it && hist_n-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);
}

void gnuish_init(struct gnuish_state *sh_state, char *const *envp)
{
	sh_state->env = envp;

	size_t env_len = 0;
	for (; *envp; ++envp)
		env_len += strlen(*envp);

	sh_state->path = envz_get(*sh_state->env, env_len, "PATH");

	sh_state->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	sh_state->cwd = malloc(_POSIX_PATH_MAX);
	sh_state->max_path = _POSIX_PATH_MAX;

	if (!getcwd(sh_state->cwd, (size_t)sh_state->max_path)) {
		/* Current working path longer than 256 chars. */

		free(sh_state->cwd);

		// We will use the buffer allocated by `getcwd`
		// to store the working directory from now on.
		// TODO: We may need to do this in `chdir` as well.
		sh_state->cwd = getcwd(NULL, 0);
		sh_state->max_path = pathconf(sh_state->cwd, _PC_PATH_MAX);
	}

	sh_state->cmd_history = sh_state->oldest_cmd = NULL;
	sh_state->hist_n = 0;

	sh_state->args = malloc(sizeof(char *) * GNUISH_MAX_ARGS);
}

size_t gnuish_read_line(struct gnuish_state *sh_state, char *out_line)
{
	gnuish_put_prompt(sh_state);

	ssize_t len = getline(&out_line, &sh_state->max_input, stdin);

	// Must remember to include null terminator!
		printf("%m\n", errno);
	// * Adding the null at index `len` INCLUDES the newline entered on the
	// terminal. So it has the effect of automatically inserting line breaks
	// when command history is printed. However, if a line with the maximum
	// length was entered, this would be indexing out of bounds.
	out_line[len - 1] = '\0';

	// TODO: Make this optional.
	// if (len > 0)
	//	// Echo line of input.
	//	write(STDOUT_FILENO, out_line, (size_t)len); // TODO: Correct
	// nbytes?

	return len;
}

void gnuish_run_cmd(struct gnuish_state *sh_state, char *line)
{
	char *pathname;

	if (strncmp(line, "r ", 2) != 0) // Recall `r` should NOT be added to
					 // history.
		gnuish_add_hist(sh_state, line);

	gnuish_parse_line(line, &pathname, sh_state->args);
	char *const filename = sh_state->args[0];

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

void gnuish_exec(struct gnuish_state *sh_state, char *pathname)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, NULL, 0);
		return;
	}

	if (!pathname) {
		char *path = malloc(strlen(sh_state->path) + 1);
		strcpy(path, sh_state->path);

		int code = -1;

		char *exec_pathname = malloc((size_t)sh_state->max_path);

		for (char *path_it = strtok(path, ":"); code == -1 && path_it;
		     path_it = strtok(NULL, ":")) {
			sprintf(exec_pathname, "%s/%s", path_it,
				sh_state->args[0]);

			code = execve(exec_pathname, sh_state->args,
				      sh_state->env);
}

		if (code == -1)
			printf("%m\n", errno);

		return;
	}

	if (execve(pathname, sh_state->args, sh_state->env) == -1)
		printf("%m\n", errno);
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
	char *const pathname = sh_state->args[1];

	realpath(pathname, sh_state->cwd);

	if (chdir(pathname) == -1) {
		printf("%m\n", errno);
		return;
	}
}