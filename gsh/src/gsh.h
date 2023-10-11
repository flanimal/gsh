#pragma once

#include <stddef.h>
#include <stdbool.h>

extern char **environ;

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

        /* The buffers used for parsing input lines. */
        struct gsh_parsed *parsed;

	struct gsh_workdir *wd;

	struct gsh_params params;

	bool show_status; // TODO: Put shell options somewhere.

        char *line; 
	size_t input_len;
};

char *gsh_getenv(const struct gsh_params *params, const char *name);

/* Set initial values and resources for the shell. */
void gsh_init(struct gsh_state *sh);

/* Get a null-terminated line of input from the terminal. */
void gsh_read_line(struct gsh_state *sh);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gsh_run_cmd(struct gsh_state *sh);