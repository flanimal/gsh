#pragma once

#include <search.h>

#include <stddef.h>
#include <stdbool.h>

#if defined(__GNUC__)
	#define unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
	#define unreachable() __assume(0)
#endif

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

/* Parameters. */
struct gsh_params {
	size_t env_len;

	/* Null-terminated value of HOME. */
	size_t home_len;

	int last_status;
};

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

	// TODO: We might have two structs,
	// struct gsh_parse_bufs
	// and
	// struct gsh_parse_state.
	// Or, we might move gsh_state's definition
	// out of the public header entirely.

	/* The buffers used for parsing. */
	struct gsh_parsed *parsed;

	struct gsh_workdir *wd;

	struct gsh_params params;

	enum gsh_shopt_flags shopts;
	struct hsearch_data *shopt_tbl;

	char *line;

	struct hsearch_data *builtin_tbl;
};

const char *gsh_getenv(const struct gsh_params *params, const char *name);

/* Set initial values and resources for the shell. */
void gsh_init(struct gsh_state *sh);

/* Execute a null-terminated line of input.
 * The line will be modified by calls to `strtok`. */
void gsh_run_cmd(struct gsh_state *sh);

void gsh_put_prompt(const struct gsh_state *sh);

void gsh_bad_cmd(const char *msg, int err);