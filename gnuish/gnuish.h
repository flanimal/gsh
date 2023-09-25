#pragma once

struct gnuish_hist_ent {
	struct gnuish_hist_ent *back, *forw;
	size_t len;
	char *line;
};

struct gnuish_state {
	/* Maximum length of newline-terminated input line on terminal. */
	long max_input;

	/* Maximum length of pathnames, including null character. */
	long max_path;

	// NOTE: If we need a buffer to get the current working directory anyway,
	// then we might as well store it in the shell state structure
	// to have on hand.
	char *cwd;

	/* Tail and head of command history queue. */
	struct gnuish_hist_ent *cmd_history, *oldest_cmd;

	/* Number of commands in history (maximum 10). */
	int hist_n;

	/* Argument buffer. */
	char **args;

	char *path;

	/* Rest of environment passed to `main`. */
	char *const *env;
};

/* Set initial values and resources for the shell. */
void gnuish_init(struct gnuish_state *sh_state, char *const *envp);

/* Get a null-terminated line of input from the terminal, 
 * including the newline. */
size_t gnuish_read_line(struct gnuish_state *sh_state, char *out_line);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gnuish_run_cmd(struct gnuish_state *sh_state, size_t len, char *line);

/* Fork and exec a program. */
void gnuish_exec(struct gnuish_state *sh_state, char *pathname);

/* 
	Shell builtins. 
*/

/* Write arguments to stdout. */
void gnuish_echo(struct gnuish_state *sh_state);

/* Change the process working directory. */
void gnuish_chdir(struct gnuish_state *sh_state);