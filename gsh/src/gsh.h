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

struct gsh_input_buf {
	// Buffer and size for getting terminal input.
	char *line;
	size_t input_len;

	// Constants relating to terminal input.
	long max_input;
	long max_path; // (?)
};

struct gsh_state {
	/* Command history. */
	struct gsh_cmd_hist *hist;

	// Currently, we have:
	// struct gsh_parsed {
	//	char **tokens;

	//	char **token_it;

	//	char **fmt_bufs;
	// };
	// 
	// This is potentially suboptimal since it ties together
	// references to the buffers for the purpose of keeping track
	// of their allocation, AND the state surrounding those buffers.
	// So if a function needs only one, it still gets both. Which potentially
	// lessens encapsulation?
	// 
	// We might instead have *two* structs,
	// 
	// struct gsh_parse_bufs {
	//	char **tokens
	//	char **fmt_bufs
	// };
	// 
	// and
	// 
	// struct gsh_parse_state {
	//	char **token_it
	//	size_t token_n;
	// 
	//	char **fmt_bufs
	// 
	//	char *lineptr?
	// };
	// 
	// This would allow us to store input_len in parse_bufs,
	// which we could pass to read_line(), 
	// 
	// But this might not make sense, since the "line" buffer is filled
	// before parsing even *begins*. It has nothing to do with the parse state!
	// 
	// What if we had:
	// 

	// 
	// We could then pass *this* to read_line().
	// TODO: Do we (need to) resize line if max_input changes?
	// 
	// 
	// /////////////////////////////////////
	// TODO: !!! Put reference to gsh_params in parse_state?
	/* The buffers used for parsing. */
	struct gsh_parse_bufs *parse_bufs;
	struct gsh_parse_state *parse_state;

	struct gsh_input_buf *input;

	/* Current working directory of the shell process. */
	char *cwd;

	struct gsh_params params;

	enum gsh_shopt_flags shopts;
	struct hsearch_data *shopt_tbl;

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

void gsh_getcwd(struct gsh_state *sh);