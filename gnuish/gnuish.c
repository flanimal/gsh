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

struct gnuish_parsed {
	/* List of tokens from previous input line. */
	char **tokens;

	/* Any dynamically allocated tokens. */
	char **alloc;
	size_t alloc_n;
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

static void gnuish_put_prompt(const struct gnuish_env *env_info,
			      const char *cwd)
{
	if (strncmp(cwd, env_info->homevar, env_info->home_len) == 0)
		printf(GNUISH_PROMPT("~%s"), cwd + env_info->home_len);
	else
		printf(GNUISH_PROMPT("%s"), cwd);
}

static void gnuish_puthelp()
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

	puts("\techo\tWrite to standard output.");
	puts("\thelp\tDisplay this help page.");

	putchar('\n');
}

static void
gnuish_parse_tok(const struct gnuish_env *env_info,
		 struct gnuish_parsed *tokens, char **const tok)
{
	// TODO: globbing, piping
	switch (**tok) {
	case '$':
		*tok = envz_get(*environ, env_info->env_len, *tok + 1);
		return;
	case '~':
		char *tok_subst = malloc(strlen(*tok) + env_info->home_len + 1);

		strcpy(stpcpy(tok_subst, env_info->homevar), *tok + 1);
		*tok = tok_subst;

		tokens->alloc[tokens->alloc_n++] = *tok;

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

/*	Returns argument list terminated with NULL, and pathname.
 *	A NULL pathname means the PATH environment variable must be used.
 */
static int gnuish_parse_line(struct gnuish_env *env_info,
			     struct gnuish_parsed *parsed,
			     char **const out_pathname, char *const line)
{
	char **const out_tokens = parsed->tokens;

	out_tokens[0] = strtok(line, " \n");
	gnuish_parse_pathname(env_info, parsed, out_pathname);

	// Get arguments.
	int arg_n;

	for (arg_n = 1; (out_tokens[arg_n] = strtok(NULL, " \n")) &&
	     arg_n <= GNUISH_MAX_ARGS; ++arg_n)
		gnuish_parse_tok(env_info, parsed, &out_tokens[arg_n]);

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

static void gnuish_list_hist(const struct gnuish_hist_ent *cmd_it)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;

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

// TODO: Possibly have a "prev_cmd" struct or something to pass to this,
// instead of the entire shell state.
/* Re-run the n-th previous line of input. */
static void gnuish_recall(struct gnuish_state *sh)
{
	struct gnuish_hist_ent *cmd_it = sh->hist->cmd_history;

	char *const recall_arg = sh->parsed->tokens[1];
	int n_arg = (recall_arg ? atoi(recall_arg) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gnuish_bad_cmd("no history", 0);
		return;
	}

	while (cmd_it && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);
	gnuish_run_cmd(sh, cmd_it->len, cmd_it->line);
}

static void gnuish_getcwd(struct gnuish_workdir *wd)
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

static void gnuish_chdir(struct gnuish_workdir *wd, const char *pathname)
{
	if (chdir(pathname) == -1)
		printf("%s\n", strerror(errno));

	gnuish_getcwd(wd);
}

size_t gnuish_max_input(const struct gnuish_state *sh)
{
	assert(g_gnuish_initialized);
	return (size_t)sh->wd->max_input;
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

	sh->parsed = malloc(sizeof(*sh->parsed));
	sh->parsed->tokens = malloc(sizeof(char *) * GNUISH_MAX_ARGS);
	sh->parsed->alloc = malloc(sizeof(char *) * GNUISH_MAX_ARGS);
	sh->parsed->alloc_n = 0;

#ifndef NDEBUG
	g_gnuish_initialized = true;
#endif
}

size_t gnuish_read_line(const struct gnuish_state *sh, char **const out_line)
{
	gnuish_put_prompt(sh->env_info, sh->wd->cwd);

	ssize_t len = getline(out_line, (size_t *)&sh->wd->max_input, stdin);

	(*out_line)[len - 1] = '\0';	// Remove newline.
	return (size_t)(len - 1);
}

void gnuish_run_cmd(struct gnuish_state *sh, size_t len, char *line)
{
	char *pathname;

	putchar('\n');
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

static int gnuish_exec_path(const char *pathvar,
			    const struct gnuish_workdir *wd, char **args)
{
	int code = -1;
	char *const exec_buf = malloc((size_t)wd->max_path);
	char *exec_pathname;

	for (const char *path_it = pathvar; *path_it;) {
		exec_pathname = exec_buf;

		copy_path_ent(&exec_pathname, &path_it);
		sprintf(exec_pathname, "/%s", args[0]);

		if (-1 != (code = execve(exec_buf, args, environ)))
			break;
	}

	free(exec_buf);

	return code;
}

// TODO: Call the functions for path and for no path directly in run_cmd above?
// Also pass sub-structs directly.
/* Fork and exec a program. */
static void gnuish_exec(const struct gnuish_state *sh, const char *pathname)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, NULL, 0);
		return;
	}

	int code = pathname ? execve(pathname, sh->parsed->tokens, environ) :
	    gnuish_exec_path(sh->env_info->pathvar, sh->wd,
			     sh->parsed->tokens);

	if (code == -1) {
		gnuish_bad_cmd((pathname ? pathname : sh->parsed->tokens[0]),
			       errno);
		exit(EXIT_FAILURE);
	}
}

void gnuish_run_cmd(struct gnuish_state *sh, size_t len, char *line)
{
	if (len == 0)
		return;

	// Recall `r` should NOT be added to history.
	if (line[0] != 'r' || (line[1] != '\0' && !isspace(line[1])))
		gnuish_add_hist(sh->hist, len, line);

	char *pathname;
	gnuish_parse_line(sh->env_info, sh->parsed, &pathname, line);

	char **const args = sh->parsed->tokens;

	// TODO: hash table or something
	if (strcmp(args[0], "cd") == 0)
		gnuish_chdir(sh->wd, args[1]);

	else if (strcmp(args[0], "r") == 0)
		gnuish_recall(sh);

	else if (strcmp(args[0], "exit") == 0)
		exit(EXIT_SUCCESS);

	else if (strcmp(args[0], "hist") == 0)
		gnuish_list_hist(sh->hist->cmd_history);

	else if (strcmp(args[0], "echo") == 0)
		gnuish_echo(args + 1);

	else if (strcmp(args[0], "help") == 0)
		gnuish_puthelp();

	else
		gnuish_exec(sh, pathname);

	// Delete dynamically allocated arguments.
	while (sh->parsed->alloc_n > 0)
		free(sh->parsed->alloc[sh->parsed->alloc_n--]);
}
