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

struct gsh_state {
	/* Command history. */
	struct gsh_cmd_hist *hist;
	
	struct gsh_input_buf *inputbuf;

	struct gsh_parse_state *parse_state;

	/* Current working directory of the shell process. */
	char *cwd;
	long max_path;

	struct gsh_params params;

	enum gsh_shopt_flags shopts;
	struct hsearch_data *shopt_tbl;

	struct hsearch_data *builtin_tbl;
};

struct gsh_parse_bufs;

struct gsh_parse_bufs *gsh_new_parsebufs();

/*	Set initial values and resources for the shell. 
 */
void gsh_init(struct gsh_state *sh, const struct gsh_parse_bufs *parsebufs);

/*	Get a zero-terminated line of input from the terminal,
 *	excluding the newline.
 */
bool gsh_read_line(struct gsh_input_buf *inputbuf);

/*	Execute a null-terminated line of input.
 */
void gsh_run_cmd(struct gsh_state *sh);

void gsh_put_prompt(const struct gsh_state *sh);

void gsh_bad_cmd(const char *msg, int err);

void gsh_getcwd(struct gsh_state *sh);