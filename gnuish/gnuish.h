#pragma once

#include <stddef.h>

/* The maximum number of arguments that can be passed on the command line. */
#define GNUISH_MAX_ARGS 64

struct gnuish_hist_ent {
	struct gnuish_hist_ent *back, *forw;
	size_t len;
	char *line;
};

// TODO: Split these sections into three structs?
struct gnuish_state {
	/*
	 *	Runtime constants.
	 */
	/* Maximum length of newline-terminated input line on terminal. */
	long max_input;

	/* Maximum length of pathnames, including null character. */
	long max_path;

	/*
	 *	Buffers.
	 */
	/* Current working directory of the shell process. */
	char *cwd;

	/* Tail and head of command history queue. */
	struct gnuish_hist_ent *cmd_history, *oldest_cmd;

	/* Number of commands in history (maximum 10). */
	int hist_n;

	/* Argument buffer. */
	char **args;

	/* Dynamically allocated arguments in argument buffer. */
	char **args_alloc;
	size_t args_alloc_n;

	/*
	 *	Environment.
	 */
	/* Null-terminated value of PATH. */
	char *pathvar;

	/* Null-terminated value of HOME. */
	char *homevar;
	size_t home_len;

	/* Rest of environment passed to `main`, null-terminated. */
	char *const *env;
	size_t env_len;
};

/* Set initial values and resources for the shell. */
void gnuish_init(struct gnuish_state *sh_state, char **const envp);

/* Get a null-terminated line of input from the terminal,
 * including the newline. */
size_t gnuish_read_line(const struct gnuish_state *sh_state, char **const out_line);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gnuish_run_cmd(struct gnuish_state *sh_state, size_t len, char *line);

/* Fork and exec a program. */
void gnuish_exec(const struct gnuish_state *sh_state, const char *pathname);

/*
 *	Shell builtins.
 */

/* Write arguments to stdout. */
void gnuish_echo(const struct gnuish_state *sh_state);

/* Change the process working directory. */
void gnuish_chdir(struct gnuish_state *sh_state);

/* Print usage information. */
void gnuish_usage();