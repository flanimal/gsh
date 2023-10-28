#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#include "parse.h"
#include "params.h"

#include "special.def"

#if defined(__GNUC__)
#define unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
#define unreachable() __assume(0)
#endif

struct gsh_parse_state {
	/* Iterator pointing to the word currently being parsed. */
	const char **word_it;
	size_t word_n;

	/* Position within word to begin expansion. */
	size_t expand_skip;

	/* Stack of buffers for words that contain substitutions. */
	size_t buf_n;
	char *wordbufs[GSH_MAX_ARGS];

	char *lineptr;
};

struct gsh_fmt_span {
	/* Beginning of format span within the current word. */
	char *begin;

	/* Length of the unformatted span, up to either the zero byte or the
	 * next special char. */
	size_t len;

	const char *fmt_str;
};

void gsh_parse_init(struct gsh_parse_state **state,
		    struct gsh_parsed_cmd **parsebufs)
{
	*parsebufs = calloc(1, sizeof(**parsebufs));

	*state = calloc(1, sizeof(**state));
	(*state)->word_it = (const char **)(*parsebufs)->argv;
}

/*	Allocate and return a word buffer.
 */
static char *gsh_alloc_wordbuf(struct gsh_parse_state *state, size_t inc)
{
	const size_t new_len = strlen(*state->word_it) + inc;
	const size_t buf_n = state->buf_n;

	char *newbuf = realloc(state->wordbufs[buf_n], new_len + 1);
	if (!state->wordbufs[buf_n])
		strcpy(newbuf, *state->word_it);

	state->wordbufs[buf_n] = newbuf;
	*state->word_it = newbuf;

	return newbuf;
}

// TODO: Keep track of length of each word?

/*	Format a span within a word with the given args, allocating a buffer if
 * necessary.
 */
static void gsh_expand_span(struct gsh_parse_state *state,
			    struct gsh_fmt_span *span, ...)
{
	va_list fmt_args;
	va_start(fmt_args, span);

	va_list args_cpy;
	va_copy(args_cpy, fmt_args);

	const int print_len = vsnprintf(NULL, 0, span->fmt_str, args_cpy);
	va_end(args_cpy);

	assert(print_len >= 0);

	// FIXME: We don't remove extra space after expansion!
	if (span->len >= (size_t)print_len) {
		state->expand_skip +=
			vsprintf(span->begin, span->fmt_str, fmt_args);

		va_end(fmt_args);
		return;
	}

	// TODO: (Idea) Allocate and fill with empty space instead of duping
	// after? Need to allocate.
	char *after = strdup(span->begin + span->len);

	const ptrdiff_t before_len = span->begin - *state->word_it;

	span->begin = gsh_alloc_wordbuf(state, print_len - span->len) + before_len;

	vsprintf(span->begin, span->fmt_str, fmt_args);
	strcpy(span->begin + print_len, after);

	state->expand_skip += (span->begin) ? (size_t)print_len : span->len;

	free(after);
	va_end(fmt_args);
}

/*	Substitute a variable reference with its value.
 *
 *	If the word consists only of the variable reference, it will be
 *	assigned to point to the value of the variable.
 *
 *	If the variable does not exist, the word will be assigned the empty
 *	string.
 */
static void gsh_fmt_var(struct gsh_parse_state *state,
			const struct gsh_params *params,
			struct gsh_fmt_span *span)
{
	if (strcmp(*state->word_it, span->begin) == 0) {
		*state->word_it = gsh_getenv(params, span->begin + 1);
		return;
	}

	char *var_name = strndup(span->begin + 1, span->len - 1);

	gsh_expand_span(state, span, gsh_getenv(params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_parse_state *state,
			  const struct gsh_params *params,
			  char *const fmt_begin)
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

		gsh_fmt_var(state, params, &span);
		break;
	}
}

/*	Substitute the home character with the value of $HOME.
*
	If the word consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_parse_state *state,
			 const struct gsh_params *params, char *const fmt_begin)
{
	const char *homevar = gsh_getenv(params, "HOME");

	// TODO: Move the whole-word check out of the fmt_* functions so that
	// it makes more sense with strpbrk() converting const char * to char *?
	if (strcmp(*state->word_it, (char[]){ GSH_HOME_CH, '\0' }) == 0) {
		*state->word_it = homevar;
		return;
	}

	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s",
	};

	gsh_expand_span(state, &span, homevar);
}

/*      Expand the last word.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_word(struct gsh_parse_state *state,
			    const struct gsh_params *params)
{
	char *fmt_begin = strpbrk(*state->word_it + state->expand_skip,
				  gsh_special_chars);

	if (!fmt_begin)
		return false;

	switch ((enum gsh_special_char)fmt_begin[0]) {
	case GSH_PARAM_CH:
		gsh_fmt_param(state, params, fmt_begin);
		return true;
	case GSH_HOME_CH:
		gsh_fmt_home(state, params, fmt_begin);
		return true;
	}

	unreachable();
}

/*      Collect and insert a fully-expanded word into the list.
 *
 *	Returns next word or NULL if no next word, similar to strtok().
 */
static const char *gsh_next_word(struct gsh_parse_state *state,
				 const struct gsh_params *params, char *line)
{
	*state->word_it = strtok_r(line, WHITESPACE, &state->lineptr);
	if (!(*state->word_it))
		return NULL;

	while (gsh_expand_word(state, params))
		;

	state->expand_skip = 0;
	if (state->wordbufs[state->buf_n])
		++state->buf_n;

	++state->word_n;
	return *state->word_it++;
}

/*      Parse the first word in the input line, and place
 *      the filename in the argument array.
 */
static bool gsh_parse_filename(struct gsh_parse_state *state,
			       const struct gsh_params *params)
{
	const char *fn = gsh_next_word(state, params, state->lineptr);
	if (!fn)
		return false;

	char *last_slash = strrchr(fn, '/');
	if (last_slash)
		state->word_it[-1] = last_slash + 1;

	return !!state->word_it[-1];
}

/*	Parse words and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 */
static void gsh_parse_cmd_args(struct gsh_parse_state *state,
			       const struct gsh_params *params)
{
	while (state->word_n <= GSH_MAX_ARGS)
		if (!gsh_next_word(state, params, NULL))
			return;
}

static void gsh_free_parsed(struct gsh_parse_state *state)
{
	state->expand_skip = 0;

	// Delete substitution buffers.
	for (; state->buf_n > 0; --state->buf_n) {
		free(state->wordbufs[state->buf_n - 1]);
		state->wordbufs[state->buf_n - 1] = NULL;
	}

	// Reset word list.
	for (; state->word_n > 0; --state->word_n)
		*(--state->word_it) = NULL;
}

// TODO: "while" builtin.
void gsh_parse_cmd(struct gsh_parsed_cmd *cmd, struct gsh_parse_state *state,
		   struct gsh_params *params, char *line)
{
	if (line[0] == '\0')
		return;

	gsh_free_parsed(state);
	state->lineptr = line;

	if (!gsh_parse_filename(state, params))
		return;

	gsh_parse_cmd_args(state, params);

	cmd->argc = state->word_n;

	// Skip any whitespace preceding pathname.
	cmd->pathname = line + strspn(line, WHITESPACE);
}
