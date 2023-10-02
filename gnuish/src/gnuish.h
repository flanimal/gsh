#pragma once

#include <unistd.h>

#include <stddef.h>

// TODO: Possibly move the "internal" structs to their own header file.

// We have one "aggregate" struct that has references to the 4 sub-structs, but
// still pass the sub-structs when they are sufficient.
// Perhaps have all the public functions (the ones declared in this file) take the aggregate struct?
// While the "internal" functions can take the sub-structs.
// This would be a reason to keep public functions _to a minimum_.
// TODO: Use separate translation units to further separate concerns? (Or pImpl?)
//      E.g. separate translation units for workdir, cmd_hist, arg_buf, etc.?
// A benefit of this current revision is that the sub-structs members cannot be accessed directly, 
// even though we have references to the structs.


// TODO: We have a separate parse state, instead of storing it in gsh_state.
// This parse state can maintain a reference to gsh_state (or to the parts of it we need).
struct gsh_state {
	/* Line history. */
	struct gsh_cmd_hist *hist;

        /* The buffers used for parsing input lines. */
        struct gsh_parsed *parsed;

        // IDEA: Execution context?
	struct gsh_workdir *wd;

        // TODO: Hashtable for important params?
        /* Parameters. */
	struct gsh_params {
		size_t env_len;

		/* Null-terminated value of PATH. */
		char *pathvar;

		/* Null-terminated value of HOME. */
		char *homevar;
		size_t home_len;

		int last_status;
	} params;
};

/* Set initial values and resources for the shell. */
void gsh_init(struct gsh_state *sh);

size_t gsh_max_input(const struct gsh_state *sh);

/* Get a null-terminated line of input from the terminal,
 * including the newline. */
ssize_t gsh_read_line(const struct gsh_state *sh, char **const out_line);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gsh_run_cmd(struct gsh_state *sh, ssize_t len, char *line);