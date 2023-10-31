#pragma once

#include <search.h>

#include <stddef.h>
#include <stdbool.h>

#include "params.h"

/*	Allocates and creates a hashtable, filling it with
 *	elements from `elems`.
 */
#define create_hashtable(elems, m_key, m_data, tbl)                            \
	do {                                                                   \
		hcreate_r(sizeof(elems) / sizeof(*elems),                      \
			  (tbl = calloc(1, sizeof(*tbl))));                    \
									       \
		ENTRY *_result;                                                \
									       \
		for (size_t _i = 0; _i < sizeof(elems) / sizeof(*elems); ++_i) \
			hsearch_r((ENTRY){ .key = elems[_i] m_key,             \
					   .data = &elems[_i] m_data },        \
				  ENTER, &_result, tbl);                       \
	} while (0)

/* Shell option bitflags. */
enum gsh_shopt_flags {
	GSH_OPT_PROMPT_WORKDIR = 1,
	GSH_OPT_PROMPT_STATUS = 2,
	GSH_OPT_ECHO = 4,
	GSH_OPT_DEFAULTS = GSH_OPT_PROMPT_WORKDIR | GSH_OPT_ECHO,
};

struct gsh_cmd_queue {
	struct gsh_parsed_cmd *front;
};

struct gsh_state {
	/* Command history. */
	struct gsh_cmd_hist *hist;
	
	struct gsh_input_buf *inputbuf;

	struct gsh_parser *parser;
	struct gsh_cmd_queue cmd_queue;

	/* Current working directory of the shell process. */
	char *cwd;

	/* The maximum length of a pathname, including the NUL byte. */
	long max_path;

	struct gsh_params params;
	enum gsh_shopt_flags shopts;
	
	struct hsearch_data *shopt_tbl;
	struct hsearch_data *builtin_tbl;
};

/*	Set initial values and resources for the shell. 
 */
void gsh_init(struct gsh_state *sh);

/*	Execute a zero-terminated line of input.
 */
void gsh_run_cmd(struct gsh_state *sh);

void gsh_put_prompt(const struct gsh_state *sh);

void gsh_bad_cmd(const char *msg, int err);

void gsh_getcwd(struct gsh_state *sh);