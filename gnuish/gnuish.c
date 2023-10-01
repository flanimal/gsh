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
#define GSH_MAX_ARGS 64

#define GSH_CMD_NOT_FOUND 127

#ifndef NDEBUG
static bool g_gsh_initialized = false;
#endif

struct gsh_workdir {
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

struct gsh_hist_ent {
	struct gsh_hist_ent *back, *forw;

	char *line;
	size_t len;
};

struct gsh_cmd_hist {
	/* Tail and head of command history queue. */
	struct gsh_hist_ent *cmd_history, *oldest_cmd;

	/* Number of commands in history (maximum 10). */
	int hist_n;
};

struct gsh_parsed {
	/* List of tokens from previous input line. */
	const char **tokens;

	/* Any dynamically allocated tokens. */
	const char **alloc;
	size_t alloc_n;
};

struct gsh_env {
	/* Null-terminated value of PATH. */
	char *pathvar;

	/* Null-terminated value of HOME. */
	char *homevar;
	size_t home_len;

	size_t env_len;
};

#define GSH_PROMPT(cwd) "\033[46m" cwd "\033[49m@ "

static void gsh_put_prompt(int last_status, const struct gsh_env *env_info,
			   const char *cwd)
{
	const bool in_home =
	    strncmp(cwd, env_info->homevar, env_info->home_len) == 0;

	printf((in_home ? "<%d>" GSH_PROMPT("~%s") :
		"<%d>" GSH_PROMPT("%s")),
	       (WIFEXITED(last_status) ? WEXITSTATUS(last_status) : 255),
	       cwd + (in_home ? env_info->home_len : 0));
}

static int gsh_puthelp()
{
	puts("\ngsh - GNU island shell");
	puts("\ngsh displays the current working directory in the shell prompt :");
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

	return 0;
}

//static const char* gsh_get_var(const struct gsh_env* env_info, const char* var)
//{
//	switch (var[0]) {
//	case '?':
//                return 
//        }
//}

static void
gsh_parse_tok(const struct gsh_env *env_info,
	      struct gsh_parsed *tokens, const char **const tok)
{
	// TODO: globbing, piping
	switch ((*tok)[0]) {
	case '$':
		*tok = envz_get(*environ, env_info->env_len, *tok + 1);
		//*tok = gsh_get_var(env_info, *tok + 1);
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

static void gsh_parse_pathname(struct gsh_env *env_info,
			       struct gsh_parsed *parsed,
			       const char **const out_pathname)
{
	const char **const out_tokens = parsed->tokens;

	// Get the pathname, whether relative or absolute, if one
	// preceded the filename.
	char *last_slash = strrchr(out_tokens[0], '/');
	if (last_slash) {
		*out_pathname = out_tokens[0];

		// Parse pathname.
		gsh_parse_tok(env_info, parsed, out_pathname);

		out_tokens[0] = last_slash + 1;
	} else {
		*out_pathname = NULL;	// TODO: Just use zero-length string instead of NULL?
	}
	// TODO: Do we need to pass both parsed and an out_tokens tok to
	// parse_tok? Parse filename.
	gsh_parse_tok(env_info, parsed, &out_tokens[0]);
}

/*	Returns argument list terminated with NULL, and pathname.
 *	A NULL pathname means the PATH environment variable must be used.
 */
static void gsh_parse_line(struct gsh_env *env_info,
			   struct gsh_parsed *parsed,
			   const char **const out_pathname, char *const line)
{
	const char **const out_tokens = parsed->tokens;

	out_tokens[0] = strtok(line, " \n");
	gsh_parse_pathname(env_info, parsed, out_pathname);

	// Get arguments.
	for (int arg_n = 1; (out_tokens[arg_n] = strtok(NULL, " \n")) &&
	     arg_n <= GSH_MAX_ARGS; ++arg_n)
		gsh_parse_tok(env_info, parsed, &out_tokens[arg_n]);
}

static void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len,
			 const char *line)
{
	// Recall `r` should NOT be added to history.
	if (line[0] == 'r' && (!line[1] || isspace(line[1])))
		return;

	struct gsh_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_hist->cmd_history);
	sh_hist->cmd_history = last_cmd;

	strcpy((last_cmd->line = malloc(len + 1)), line);
	last_cmd->len = len;

	if (sh_hist->hist_n == 10) {
		struct gsh_hist_ent *popped_ent = sh_hist->oldest_cmd;
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

static int gsh_list_hist(const struct gsh_hist_ent *cmd_it)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;

	for (; cmd_it; cmd_it = cmd_it->forw)
		printf("%i: %s\n", cmd_n++, cmd_it->line);

	return 0;
}

static void gsh_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n",
	       (msg ? ":" : ""),
	       (msg ? msg : ""),
	       (err ? "(" : ""), (err ? strerror(err) : ""), (err ? ")" : ""));
}

// TODO: Possibly have a "prev_cmd" struct or something to pass to this,
// instead of the entire shell state.
/* Re-run the n-th previous line of input. */
static int gsh_recall(struct gsh_state *sh, const char *recall_arg)
{
	struct gsh_hist_ent *cmd_it = sh->hist->cmd_history;

	int n_arg = (recall_arg ? atoi(recall_arg) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gsh_bad_cmd("no history", 0);
		return -1;
	}

	while (cmd_it && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);
	gsh_run_cmd(sh, cmd_it->len, cmd_it->line);
	return sh->last_status;
}

static void gsh_getcwd(struct gsh_workdir *wd)
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

static int gsh_chdir(struct gsh_workdir *wd, const char *pathname)
{
	if (chdir(pathname) == -1) {
		printf("%s\n", strerror(errno));
		return -1;
	}

	gsh_getcwd(wd);

	return 0;
}

size_t gsh_max_input(const struct gsh_state *sh)
{
	assert(g_gsh_initialized);
	return (size_t)sh->wd->max_input;
}

static void gsh_init_env(struct gsh_env *env_info)
{
	env_info->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		env_info->env_len += strlen(*env_it);

	env_info->pathvar = envz_get(*environ, env_info->env_len, "PATH");

	env_info->homevar = envz_get(*environ, env_info->env_len, "HOME");
	env_info->home_len = strlen(env_info->homevar);
}

static void gsh_init_wd(struct gsh_workdir *wd)
{
	// Get working dir and its max path length.
	wd->cwd = malloc((size_t)(wd->max_path = _POSIX_PATH_MAX));
	gsh_getcwd(wd);

	// Get maximum length of terminal input line.
	wd->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);
}

void gsh_init(struct gsh_state *sh)
{
	gsh_init_env((sh->env_info = malloc(sizeof(*sh->env_info))));
	gsh_init_wd((sh->wd = malloc(sizeof(*sh->wd))));

        sh->parsed = malloc(sizeof(*sh->parsed));
	sh->parsed->tokens = malloc(sizeof(char *) * GSH_MAX_ARGS);
	sh->parsed->alloc = malloc(sizeof(char *) * GSH_MAX_ARGS);
	sh->parsed->alloc_n = 0;

	sh->hist = malloc(sizeof(*sh->hist));
	sh->hist->cmd_history = sh->hist->oldest_cmd = NULL;
	sh->hist->hist_n = 0;

	sh->last_status = 0;

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

ssize_t gsh_read_line(const struct gsh_state *sh, char **const out_line)
{
	gsh_put_prompt(sh->last_status, sh->env_info, sh->wd->cwd);

	ssize_t len = getline(out_line, (size_t *)&sh->wd->max_input, stdin);

	(*out_line)[len - 1] = '\0';	// Remove newline.
	return len - 1;
}

static int gsh_echo(const char *const *args)
{
	for (; *args; putchar(' '), ++args)
		if (fputs(*args, stdout) == EOF)
			return -1;

	putchar('\n');

	return 0;
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

static int gsh_exec_path(const char *pathvar,
			 const struct gsh_workdir *wd, const char **args)
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
static int gsh_exec(struct gsh_state *sh, const char *pathname, const char **args)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		waitpid(cmd_pid, &sh->last_status, 0);
		return sh->last_status;
	}

	if (pathname)
		execve(pathname, (char *const *)args, environ);
	else
		gsh_exec_path(sh->env_info->pathvar, sh->wd,
			      args);

	// Named program couldn't be executed.
	gsh_bad_cmd((pathname ? pathname : args[0]), errno);
	exit(GSH_CMD_NOT_FOUND);
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

static void gsh_free_parsed(struct gsh_parsed *parsed)
{
	// Delete any token substitution buffers.
	while (parsed->alloc_n > 0)
		free((char *)parsed->alloc[parsed->alloc_n--]);
}

void gsh_run_cmd(struct gsh_state *sh, size_t len, char *line)
{
	if (len == 0)
		return;

	gsh_add_hist(sh->hist, len, line);

	const char *pathname;
	gsh_parse_line(sh->env_info, sh->parsed, &pathname, line);

	sh->last_status = gsh_switch(sh, pathname, sh->parsed->tokens);

	gsh_free_parsed(sh->parsed);
}
