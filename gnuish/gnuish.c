#include <unistd.h>
#include <limits.h>
#include <search.h>
#include <envz.h>
#include <sys/wait.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "gnuish.h"

extern char **environ;

/* The maximum number of arguments that can be passed on the command line. */
#define GNUISH_MAX_ARGS 64

#ifndef NDEBUG
bool g_gnuish_initialized = false;
#endif

struct gnuish_workdir {
	/* Current working directory of the shell process. */
	char *cwd;

	/*
	 *      Runtime constants.
	 */
	/* Maximum length of newline-terminated input line on terminal. */
	long max_input;

	/* Maximum length of pathnames, including null character. */
	long max_path;
};

struct gnuish_hist_ent {
	struct gnuish_hist_ent *back, *forw;

	char *line;
	size_t len;
};

struct gnuish_cmd_hist {
	/* Tail and head of command history queue. */
	struct gnuish_hist_ent *cmd_history, *oldest_cmd;

	/* Number of commands in history (maximum 10). */
	int hist_n;
};

struct gnuish_arg_buf {
	/* Argument buffer. */
	char **args;

	/* Dynamically allocated arguments in argument buffer. */
	char **args_alloc;
	size_t args_alloc_n;
};

struct gnuish_env {
	/* Null-terminated value of PATH. */
	char *pathvar;

	/* Null-terminated value of HOME. */
	char *homevar;
	size_t home_len;

	size_t env_len;
};

#define GNUISH_PROMPT(cwd) "\033[46m" cwd "\033[49m@ "

static void gnuish_put_prompt(const struct gnuish_workdir *sh_wd,
{
	if (strncmp(sh_wd->cwd, sh_env->homevar, sh_env->home_len) == 0)
		printf(GNUISH_PROMPT("~%s"), sh_wd->cwd + sh_env->home_len);
	else
		printf(GNUISH_PROMPT("%s"), sh_wd->cwd);
}

static void gnuish_puthelp()
			     const struct gnuish_env *sh_env, char **const arg)
{
	// TODO: globbing, piping
	switch (**arg) {
	case '$':
		*tok = envz_get(*environ, env_info->env_len, *tok + 1);
		return;
	case '~':
		char *arg_subst = malloc(strlen(*arg) + sh_env->home_len + 1);

		strcpy(stpcpy(arg_subst, sh_env->homevar), *arg + 1);
		*arg = arg_subst;

		sh_args->args_alloc[sh_args->args_alloc_n++] = *arg;

		return;
	default:
		return;
	}
}

static void gnuish_parse_pathname(struct gnuish_env *env_info,
				  struct gnuish_parsed *parsed,
				  char **const out_pathname)
{
	char **const out_tokens = parsed->tokens;

		// Get the pathname, whether relative or absolute, if one
		// preceded the filename.
	char *last_slash = strrchr(out_tokens[0], '/');
		if (last_slash) {
		*out_pathname = out_tokens[0];

			// Parse pathname.
		gnuish_parse_tok(env_info, parsed, out_pathname);

		out_tokens[0] = last_slash + 1;
		} else {
		*out_pathname = NULL;	// TODO: Just use zero-length string instead of NULL?
	}
	// TODO: Do we need to pass both parsed and an out_tokens tok to
	// parse_tok? Parse filename.
	gnuish_parse_tok(env_info, parsed, &out_tokens[0]);
		}

		// Parse filename.
		gnuish_parse_tok(sh_args, sh_env, &out_args[0]);
	}

	gnuish_parse_pathname(env_info, parsed, out_pathname);
	// Get arguments.
	int arg_n;

	for (arg_n = 1; (out_args[arg_n] = strtok(NULL, " \n")) &&
	     arg_n <= GNUISH_MAX_ARGS; ++arg_n)
		gnuish_parse_tok(sh_args, sh_env, &out_args[arg_n]);

	return arg_n + 1;
}

static void gnuish_add_hist(struct gnuish_cmd_hist *sh_hist, size_t len,
			    const char *line)
{
	struct gnuish_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_hist->cmd_history);
	sh_hist->cmd_history = last_cmd;

	strcpy((last_cmd->line = malloc(len + 1)), line);
	last_cmd->len = len;

	if (sh_hist->hist_n == 10) {
		struct gnuish_hist_ent *popped_ent = sh_hist->oldest_cmd;
		sh_hist->oldest_cmd = popped_ent->back;

		remque(popped_ent);

		free(popped_ent->line);
		free(popped_ent);

		return;
	}

	if (sh_hist->hist_n == 0)
		sh_hist->oldest_cmd = last_cmd;

	++sh_hist->hist_n;
}

static void gnuish_list_hist(const struct gnuish_cmd_hist *sh_hist)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;
	struct gnuish_hist_ent *cmd_it = sh_hist->cmd_history;

	for (; cmd_it; cmd_it = cmd_it->forw)
		printf("%i: %s\n", cmd_n++, cmd_it->line);
}

static void gnuish_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n",
	       (msg ? ":" : ""),
	       (msg ? msg : ""),
	       (err ? "(" : ""), (err ? strerror(err) : ""), (err ? ")" : ""));
}

/* Re-run the n-th previous line of input. */
static void gnuish_recall(struct gnuish_state *sh)
{
	struct gnuish_hist_ent *cmd_it = sh->hist->cmd_history;

	int n_arg = (sh->arg_buf->args[1] ? atoi(sh->arg_buf->args[1]) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gnuish_bad_cmd("no history", 0);
		return;
	}

	while (cmd_it && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);
	gnuish_run_cmd(sh, cmd_it->len, cmd_it->line);
}

static void gnuish_getcwd(struct gnuish_workdir *sh_wd)
{
	if (getcwd(sh_wd->cwd, (size_t)sh_wd->max_path))
		return;

	/* Current working path longer than max_path chars. */
	free(sh_wd->cwd);

	// We will use the buffer allocated by `getcwd`
	// to store the working directory from now on.
	sh_wd->cwd = getcwd(NULL, 0);
	sh_wd->max_path = pathconf(sh_wd->cwd, _PC_PATH_MAX);
}

static void gnuish_init_env(struct gnuish_env *sh_env, char **envp)
{
	sh_env->envz = envp;
	sh_env->env_len = 0;

	for (; *envp; ++envp)
		sh_env->env_len += strlen(*envp);

	sh_env->pathvar = envz_get(*sh_env->envz, sh_env->env_len, "PATH");

	sh_env->homevar = envz_get(*sh_env->envz, sh_env->env_len, "HOME");
	sh_env->home_len = strlen(sh_env->homevar);
}

size_t gnuish_max_input(const struct gnuish_state *sh_state)
{
	assert(g_gnuish_initialized);
}

void gnuish_init(struct gnuish_state *sh, char **const envp)
{
	env_info->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		env_info->env_len += strlen(*env_it);

	env_info->pathvar = envz_get(*environ, env_info->env_len, "PATH");

	env_info->homevar = envz_get(*environ, env_info->env_len, "HOME");
	env_info->home_len = strlen(env_info->homevar);
}

static void gnuish_init_wd(struct gnuish_workdir* wd)
{
	// Get working dir and its max path length.
	sh->workdir->cwd =
	    malloc((size_t)(sh->workdir->max_path = _POSIX_PATH_MAX));
	gnuish_getcwd(sh->workdir);

	// Get maximum length of terminal input line.
	sh->workdir->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	sh->hist->cmd_history = sh->hist->oldest_cmd = NULL;
	sh->hist->hist_n = 0;

	sh->arg_buf->args = malloc(sizeof(char *) * GNUISH_MAX_ARGS);
	sh->arg_buf->args_alloc = malloc(sizeof(char *) * GNUISH_MAX_ARGS);
	sh->arg_buf->args_alloc_n = 0;
#ifndef NDEBUG
	g_gnuish_initialized = true;
#endif
}

size_t gnuish_read_line(const struct gnuish_state *sh, char **const out_line)
{
	gnuish_put_prompt(sh->workdir, sh->env);

	ssize_t len =
	    getline(out_line, (size_t *)&sh->workdir->max_input, stdin);

	ssize_t len = getline(out_line, (size_t *)&sh->wd->max_input, stdin);

	(*out_line)[len - 1] = '\0';	// Remove newline.
	return (size_t)(len - 1);
}

void gnuish_run_cmd(struct gnuish_state *sh, size_t len, char *line)
{
	char *pathname;

	// Recall `r` should NOT be added to history.
	if (!(line[0] == 'r' && (line[1] == '\0' || isspace(line[1]))))
		gnuish_add_hist(sh->hist, len, line);

	if (-1 == gnuish_parse_line(sh->arg_buf, sh->env, line,
				    &pathname, sh->arg_buf->args))
		// The line is empty.
		return;

	const char *const filename = sh->arg_buf->args[0];

	// TODO: hash table or something
	if (strcmp(filename, "cd") == 0)
		gnuish_chdir(sh);

	else if (strcmp(filename, "r") == 0)
		gnuish_recall(sh);

	else if (strcmp(filename, "exit") == 0)
		exit(EXIT_SUCCESS);

	else if (strcmp(filename, "hist") == 0)
		gnuish_list_hist(sh->hist);

	else if (strcmp(filename, "echo") == 0)
		gnuish_echo(sh);

	else if (strcmp(filename, "help") == 0)
		gnuish_usage();

	else
		gnuish_exec(sh, pathname);

	// Delete dynamically allocated arguments.
	while (sh->arg_buf->args_alloc_n > 0)
		free(sh->arg_buf->args_alloc[sh->arg_buf->args_alloc_n--]);
}

/* Copy a null-terminated path from PATH variable, stopping when a colon ':' or
 * null terminator is encountered. */
static void gnuish_copy_path(char **const exec_it, char **const path_it)
{
	for (;; ++(*exec_it), ++(*path_it)) {
		switch (**path_it) {
		case ':':
			++(*path_it);	// Move to next path following the colon.

			/* fall through */
		case '\0':
			**exec_it = '\0';
			return;
		default:
			**exec_it = **path_it;
			continue;
		}
	}
}

static int gnuish_exec_path(const struct gnuish_workdir *sh_wd,
			    const struct gnuish_arg_buf *sh_args,
			    const struct gnuish_env *sh_env)
{
	int code = -1;
	char *const exec_buf = malloc((size_t)sh_wd->max_path);

	for (char *path_it = sh_env->pathvar; *path_it;) {
		char *exec_pathname = exec_buf;

		gnuish_copy_path(&exec_pathname, &path_it);
		sprintf(exec_pathname, "/%s", sh_args->args[0]);

		if (-1 !=
		    (code = execve(exec_buf, sh_args->args, sh_env->envz)))
			break;
	}

	free(exec_buf);

	return code;
}

void gnuish_exec(const struct gnuish_state *sh, const char *pathname)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, NULL, 0);
		return;
	}

	if (-1 ==
	    (pathname ? execve(pathname, sh->arg_buf->args, sh->env->envz) :
	     gnuish_exec_path(sh->workdir, sh->arg_buf, sh->env))) {
		gnuish_bad_cmd(errno);
		gnuish_bad_cmd((pathname ? pathname : sh->parsed->tokens[0]),
			       errno);
		exit(EXIT_FAILURE);
	}
}

void gnuish_echo(const struct gnuish_state *sh)
{
	char *const *args = sh->arg_buf->args;

	// Increment at start to skip name of builtin.
	for (++args; *args; ++args)
		printf("%s ", *args);

	putchar('\n');
}

void gnuish_chdir(struct gnuish_state *sh)
{
	const char *const pathname = sh->arg_buf->args[1];

	if (chdir(pathname) == -1)
		printf("%s\n", strerror(errno));

	gnuish_getcwd(sh->workdir);
}

void gnuish_usage()
{
	puts("\ngnuish - GNU island shell");
	puts("\ngnuish displays the current working directory in the shell prompt :");
	puts("\t~@ /");
	puts("\tusr/ @");
	puts("\t/mnt/.../repos @");

	puts("\nCommands");
	puts("\n\t<command> [<arg>...]\tRun command or program with optional arguments.");

	puts("\n\tr[<n>]\tExecute the nth last line.");
	puts("\t\tThe line will be placed in history--not the `r` invocation.");
	puts("\t\tThe line in question will be echoed to the screen before being executed.");

	puts("\nShell builtins");
	puts("\n\texit\tExit the shell.");
	puts("\thist\tDisplay up to 10 last lines entered, numbered.");

	puts("\t----");
		gnuish_puthelp();

	puts("\techo\tWrite to standard output.");
	puts("\thelp\tDisplay this help page.");

	putchar('\n');
}
