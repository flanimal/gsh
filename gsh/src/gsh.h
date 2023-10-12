#pragma once

#include <stddef.h>
#include <stdbool.h>

/* Parameters. */
struct gsh_params {
	size_t env_len;

	/* Null-terminated value of HOME. */
	size_t home_len;

	int last_status;
};

struct gsh_state {
	/* Line history. */
	struct gsh_cmd_hist *hist;

        /* Line parse state. */
        struct gsh_parsed *parsed;

	struct gsh_workdir *wd;

	struct gsh_params params;

	bool show_status; // TODO: Put shell options somewhere.

        char *line; 
	size_t input_len; // TODO: (!) Replace with line_it? Even move line into parse state?
};

char *gsh_getenv(const struct gsh_params *params, const char *name);

/* Set initial values and resources for the shell. */
void gsh_init(struct gsh_state *sh);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gsh_run_cmd(struct gsh_state *sh);

void gsh_put_prompt(const struct gsh_state *sh);