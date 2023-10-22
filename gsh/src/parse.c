#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#include "parse.h"

#include "special.def"

#if defined(__GNUC__)
	#define unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
	#define unreachable() __assume(0)
#endif

// FIXME: Put somewhere else!!!
/* Parameters. */
struct gsh_params {
	size_t env_len;

	/* Null-terminated value of HOME. */
	size_t home_len;

	int last_status;
};

// FIXME: And this!!!
const char *gsh_getenv(const struct gsh_params *params, const char *name);

/*
 *	The maximum number of arguments that can be passed on the command line,
 * 	including the filename.
 */
#define GSH_MAX_ARGS 64

struct gsh_parse_bufs {
	/* List of tokens to be returned from parsing. */
	char **tokens;

	/* Stack of buffers for tokens that contain substitutions. */
	char **fmt_bufs;
};

struct gsh_parse_state {
	/* Iterator pointing to the token currently being parsed. */
	char **token_it;

	size_t token_n;

	char **fmt_bufs;

	char *lineptr;
};

struct gsh_fmt_span {
	/* Beginning of format span within the current token. */
	char *begin;

	/* Length of the unformatted span, up to either the zero byte or the
	 * next special char. */
	size_t len;

	/* A copy of the token text following the span, if any. */
	char *after;

	const char *fmt_str;
};

struct gsh_parse_bufs *gsh_init_parsebufs()
{
	struct gsh_parse_bufs *parsed = malloc(sizeof(*parsed));

	parsed->tokens = calloc(GSH_MAX_ARGS, sizeof(char *));

	// MAX_ARGS plus sentinel.
	parsed->fmt_bufs = calloc(GSH_MAX_ARGS + 1, sizeof(char *));

	return parsed;
}

void gsh_set_parse_state(struct gsh_parse_bufs *parse_bufs, struct gsh_parse_state **state)
{
	*state = malloc(sizeof(**state));

	(*state)->fmt_bufs = parse_bufs->fmt_bufs;
	(*state)->token_it = parse_bufs->tokens;
	(*state)->token_n = 0;
}

/*	Allocate and return a new format buffer.
 */
static char *gsh_alloc_fmtbuf(struct gsh_parse_state *state, size_t new_len)
{
	if (state->fmt_bufs[1]) {
		state->fmt_bufs[1] = realloc(state->fmt_bufs[1], new_len + 1);
	} else {
		state->fmt_bufs[1] = malloc(new_len + 1);

		strcpy(state->fmt_bufs[1], *state->token_it);
	}
	// There is currently no way to know whether to allocate
	// or reallocate the buffer unless we increment fmt_bufs OUTSIDE of
	// expand_tok().
	// I think a confusion came from the fact that only ONE buffer
	// will ever exist for a token/"word". There will never be multiple.

	return state->fmt_bufs[1];
}

/*	Copy the token to a buffer for expansion.
 */
static char *gsh_expand_alloc(struct gsh_parse_state *state,
			      struct gsh_fmt_span *span, size_t print_len)
{
	span->begin[0] = '\0';

	const size_t before_len = strlen(*state->token_it);

	char *fmtbuf = gsh_alloc_fmtbuf(state, before_len + print_len +
						       strlen(span->after));

	*state->token_it = fmtbuf;
	fmtbuf += before_len;

	return fmtbuf;
}

/*	Format a span with the given args, allocating a buffer if necessary.
 */
static void gsh_expand_span(struct gsh_parse_state *state,
			    struct gsh_fmt_span *span, ...)
{
	va_list fmt_args;
	va_start(fmt_args, span);

	va_list args_cpy;
	va_copy(args_cpy, fmt_args);

	const int print_len =
		vsnprintf(span->begin, span->len, span->fmt_str, args_cpy);
	va_end(args_cpy);

	assert(print_len >= 0);

	span->after = span->begin[span->len] ? strdup(span->begin + span->len) :
					       "";

	if (span->len < (size_t)print_len) {
		// Need to allocate.
		span->begin = gsh_expand_alloc(state, span, (size_t)print_len);
		vsprintf(span->begin, span->fmt_str, fmt_args);
	}

	va_end(fmt_args);
	strcpy(span->begin + print_len, span->after);

	if (strcmp(span->after, "") != 0)
		free(span->after);
}

/*	Substitute a variable reference with its value.
 *
 *	If the token consists only of the variable reference, it will be
 *	assigned to point to the value of the variable.
 *
 *	If the variable does not exist, the token will be assigned the empty
 *	string.
 */
static void gsh_fmt_var(struct gsh_params *params,
			struct gsh_parse_state *state,
			struct gsh_fmt_span *span)
{
	if (strcmp(*state->token_it, span->begin) == 0) {
		*state->token_it = (char *)gsh_getenv(params, span->begin + 1);
		return;
	}

	char *var_name = strndup(span->begin + 1, span->len - 1);

	gsh_expand_span(state, span, gsh_getenv(params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_params *params,
			  struct gsh_parse_state *state, char *const fmt_begin)
{
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1, gsh_special_chars) + 1,
	};

	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_STATUS_PARAM:
		span.fmt_str = "%d";

		gsh_expand_span(state, &span, params->last_status);
		break;
	default:
		span.fmt_str = "%s";

		gsh_fmt_var(params, state, &span);
		break;
	}
}

/*	Substitute the home character with the value of $HOME.
*
	If the token consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_params *params,
			 struct gsh_parse_state *state, char *const fmt_begin)
{
	const char *homevar = gsh_getenv(params, "HOME");

	if (strcmp(*state->token_it, (char[]){ GSH_HOME_CH, '\0' }) == 0) {
		*state->token_it = (char *)homevar;
		return;
	}

	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s",
	};

	gsh_expand_span(state, &span, homevar);
}

/*      Expand the last token.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_tok(struct gsh_params *params,
			   struct gsh_parse_state *state)
{
	char *fmt_begin = strpbrk(*state->token_it, gsh_special_chars);

	if (!fmt_begin)
		return false;

	switch ((enum gsh_special_char)fmt_begin[0]) {
	case GSH_PARAM_CH:
		gsh_fmt_param(params, state, fmt_begin);
		return true;
	case GSH_HOME_CH:
		gsh_fmt_home(params, state, fmt_begin);
		return true;
	}

	unreachable();
}

/*      Collect and insert a fully-expanded token into the list.
 *
 *	Returns next token or NULL if no next token, similar to strtok().
 */
static char *gsh_next_tok(struct gsh_params *params,
			  struct gsh_parse_state *state, char *line)
{
	char *next_tok = strtok_r(line, " ", &state->lineptr);
	if (!next_tok)
		return NULL;

	*state->token_it = next_tok;

	while (gsh_expand_tok(params, state))
		;

	if (state->fmt_bufs[1])
		++state->fmt_bufs;

	++state->token_it;
	++state->token_n;
	return next_tok;
}

/*      Parse the first token in the input line, and place
 *      the filename in the argument array.
 */
static bool gsh_parse_filename(struct gsh_params *params,
			       struct gsh_parse_state *state)
{
	char *fn = gsh_next_tok(params, state, state->lineptr);
	if (!fn)
		return false;

	char *last_slash = strrchr(fn, '/');
	if (last_slash)
		state->token_it[-1] = last_slash + 1;

	return !!state->token_it[-1];
}

/*	Parse tokens and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 */
static void gsh_parse_cmd_args(struct gsh_params *params,
			       struct gsh_parse_state *state)
{
	while (state->token_n <= GSH_MAX_ARGS)
		if (!gsh_next_tok(params, state, NULL))
			return;
}

static void gsh_free_parsed(struct gsh_parse_state *state)
{
	// Delete substitution buffers.
	while (*state->fmt_bufs) {
		free(*state->fmt_bufs);
		*state->fmt_bufs-- = NULL;
	}

	// Reset token list.
	for (; state->token_n > 0; --state->token_n)
		*(--state->token_it) = NULL;
}

// TODO: "while" builtin.
char **gsh_parse_cmd(struct gsh_params *params, struct gsh_parse_state *parse_state, char **line)
{
	if ((*line)[0] == '\0')
		return NULL;

	gsh_free_parsed(parse_state);
	parse_state->lineptr = *line;

	char **const tokens = parse_state->token_it;

	if (!gsh_parse_filename(params, parse_state))
		return NULL;

	gsh_parse_cmd_args(params, parse_state);

	// Skip any whitespace preceding pathname.
	*line += strspn(*line, " ");

	return tokens;
}
