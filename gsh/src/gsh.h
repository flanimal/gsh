#pragma once

#include <stddef.h>

/* The maximum number of arguments that can be passed on the command line. */
#define GSH_MAX_ARGS 64

#define GSH_PROMPT(cwd) "\033[46m" cwd "\033[49m@ "
#define GSH_SECOND_PROMPT "> "

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

        char *line; 
	size_t input_len;
};

size_t gsh_max_input(const struct gsh_state *sh);

void gsh_put_prompt(const struct gsh_state *sh);

char *gsh_getenv(const struct gsh_params *params, const char *name);

/* Set initial values and resources for the shell. */
void gsh_init(struct gsh_state *sh);

/* Get a null-terminated line of input from the terminal. */
void gsh_read_line(struct gsh_state *sh);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gsh_run_cmd(struct gsh_state *sh);