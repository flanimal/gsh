#pragma once

// TODO: Split these sections into three structs? Separate the concerns.
// TODO: Possibly move the "internal" structs to their own header file.

// Working directory struct?
// NOTE: We have one "aggregate" struct that has references to the 4 sub-structs, but
// still pass the sub-structs when they are sufficient.
// Perhaps have all the public functions (the ones declared in this file) take the aggregate struct?
// While the "internal" functions can take the sub-structs.
// This would be a reason to keep public functions _to a minimum_.

struct gnuish_state {
	struct gnuish_workdir *workdir;
	struct gnuish_cmd_hist *hist;
	struct gnuish_arg_buf *arg_buf;
	struct gnuish_env *env;
};

/* Set initial values and resources for the shell. */
void gnuish_init(struct gnuish_state *sh, char **const envp);

/* Get a null-terminated line of input from the terminal,
 * including the newline. */
size_t gnuish_read_line(const struct gnuish_state *sh, char **const out_line);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gnuish_run_cmd(struct gnuish_state *sh, size_t len, char *line);

/* Fork and exec a program. */
void gnuish_exec(const struct gnuish_state *sh, const char *pathname);

size_t gnuish_max_input(const struct gnuish_state *sh);

/*
 *	Shell builtins.
 */

/* Write arguments to stdout. */
void gnuish_echo(const struct gnuish_state *sh);

/* Change the process working directory. */
void gnuish_chdir(struct gnuish_state *sh);

/* Print usage information. */
void gnuish_usage();
