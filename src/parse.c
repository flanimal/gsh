#include <limits.h>

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

void gsh_set_opt(struct gsh_state *sh, char *name, bool value);

struct gsh_parse_state {
	/* Iterator pointing to the word currently being parsed. */
	const char **word_it;
	size_t word_n;

	/* Must be below ARG_MAX/__POSIX_ARG_MAX. */
	size_t words_size;

	/* Pointer to the next word in the line, gotten by strtok_r(). */
	char *lineptr;
};

struct gsh_expand_state
{
	/* Position within word to begin expansion. */
	size_t expand_skip;

	const struct gsh_params *params;

	/* Stack of buffers for words that contain substitutions. */
	size_t buf_n;
	char *wordbufs[];
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
		    struct gsh_parsed_cmd **parsebufs,
		    struct gsh_params *params)
{
	const size_t args_size = GSH_MIN_ARG_N * sizeof(char *);

	*parsebufs = calloc(1, sizeof(**parsebufs) + args_size);

	*state = calloc(1, sizeof(**state) + args_size);
	(*state)->word_it = (const char **)(*parsebufs)->argv;
	(*state)->params = params;
}

/*	Allocate and return a word buffer.
 */
static char *gsh_alloc_wordbuf(struct gsh_parse_state *state, int inc)
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

/*	Format a span within a word with the given args, allocating a buffer if
 *	necessary.
 */
static void gsh_expand_span(struct gsh_parse_state *state,
			    struct gsh_fmt_span *span, ...)
{
	va_list fmt_args;

	va_start(fmt_args, span);
	// TODO: MAX_EXPAND_LEN?
	const int print_len = vsnprintf(NULL, 0, span->fmt_str, fmt_args);
	va_end(fmt_args);

	assert(print_len >= 0);
	const int size_inc = print_len - span->len;

	va_start(fmt_args, span);
	// FIXME: We don't remove extra space after expansion!
	if (size_inc <= 0) {
		state->expand_skip +=
			vsprintf(span->begin, span->fmt_str, fmt_args);

		va_end(fmt_args);
		return;
	}

	state->words_size += size_inc;
	
	const ptrdiff_t before_len = span->begin - *state->word_it;

	// TODO: (Idea) Allocate and fill with empty space instead of duping
	// after? Need to allocate.
	char *after = strdup(span->begin + span->len);
	span->begin = gsh_alloc_wordbuf(state, size_inc) + before_len;

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
			struct gsh_fmt_span *span)
{
	if (strcmp(*state->word_it, span->begin) == 0) {
		*state->word_it = gsh_getenv(state->params, span->begin + 1);
		return;
	}

	char *var_name = strndup(span->begin + 1, span->len - 1);

	gsh_expand_span(state, span, gsh_getenv(state->params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_parse_state *state,
			  char *const fmt_begin)
{
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1, gsh_special_chars) + 1,
	};

	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_CHAR_PARAM_STATUS:
		span.fmt_str = "%d";

		gsh_expand_span(state, &span, state->params->last_status);
		break;
	default:
		span.fmt_str = "%s";

		gsh_fmt_var(state, &span);
		break;
	}
}

/*	Substitute the home character with the value of $HOME.
*
	If the word consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_parse_state *state, char *const fmt_begin)
{
	const char *homevar = gsh_getenv(state->params, "HOME");

	// TODO: Move the whole-word check out of the fmt_* functions so that
	// it makes more sense with strpbrk() converting const char * to char *?
	if (strcmp(*state->word_it, (char[]){ GSH_CHAR_HOME, '\0' }) == 0) {
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
static bool gsh_expand_word(struct gsh_parse_state *state)
{
	char *fmt_begin = strpbrk(*state->word_it + state->expand_skip,
				  gsh_special_chars);

	if (!fmt_begin)
		return false;

	switch ((enum gsh_special_char)fmt_begin[0]) {
	case GSH_CHAR_PARAM:
		gsh_fmt_param(state, fmt_begin);
		return true;
	case GSH_CHAR_HOME:
		gsh_fmt_home(state, fmt_begin);
		return true;
	}

	unreachable();
}

/*      Collect and insert a fully-expanded word into the list.
 *
 *	Returns next word or NULL if no next word, similar to strtok().
 */
static const char *gsh_next_word(struct gsh_parse_state *state, char *line)
{
	*state->word_it = strtok_r(line, WHITESPACE, &state->lineptr);
	if (!(*state->word_it))
		return NULL;

	const size_t old_words_size = state->words_size;

	// FIXME: ... is using lineptr a good idea?
	state->words_size += state->lineptr - *state->word_it;

	while (gsh_expand_word(state))
		;

	const size_t word_len = state->words_size - old_words_size;

	if (*state->word_it[word_len - 1] == ';')
		// End current command, and begin another with remaining
		// words.
		//
		// This could be done by checking that we have reached
		// the end of the input buffer; if not, it must mean
		// there was more than one command.
		return;

	state->expand_skip = 0;
	if (state->wordbufs[state->buf_n])
		++state->buf_n;

	++state->word_n;
	return *state->word_it++;
}

/*      Parse the first word in the input line, and place
 *      the filename in the argument array.
 */
//static bool gsh_parse_filename(struct gsh_parse_state *state,
//			       const struct gsh_params *params)
//{
//	const char *fn = gsh_next_word(state, params, state->lineptr);
//	if (!fn)
//		return false;
//
//	char *last_slash = strrchr(fn, '/');
//	if (last_slash)
//		state->word_it[-1] = last_slash + 1;
//
//	return !!state->word_it[-1];
//}
//
///*	Parse words and place them into the argument array, which is
// *      then terminated with a NULL pointer.
// */
//static void gsh_parse_cmd_args(struct gsh_parse_state *state,
//			       const struct gsh_params *params)
//{
//	while (state->words_size <= (_POSIX_ARG_MAX - params->env_len))
//		if (!gsh_next_word(state, params, NULL))
//			return;
//}

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

// TODO: Make process_opt a builtin?
/*
	You don't want to have to specify explicitly what to do if
	a token or part of token isn't found. It's verbose and clumsy.
*/
static void gsh_process_opt(struct gsh_state *sh, char *shopt_ch)
{
	if (!isalnum(shopt_ch[1])) {
		// There wasn't a name following the '@' character,
		// so remove the '@' and continue.
		*shopt_ch = ' ';
		return;
	}

	char *valstr = strpbrk(shopt_ch + 1, WHITESPACE);
	char *after = valstr;

	if (valstr && isalpha(valstr[1])) {
		*valstr++ = '\0';

		const int val = (strncmp(valstr, "on", 2) == 0)	 ? true :
				(strncmp(valstr, "off", 3) == 0) ? false :
								   -1;
		if (val != -1) {
			after = strpbrk(valstr, WHITESPACE);
			gsh_set_opt(sh, shopt_ch + 1, val);
		}
	}

	if (!after) {
		*shopt_ch = '\0';
		return;
	}

	while (shopt_ch != after + 1)
		*shopt_ch++ = ' ';
}

void gsh_split_words(struct gsh_parse_state *state, char *line)
{
	gsh_free_parsed(state);
	state->lineptr = line;

	while (state->words_size <= (_POSIX_ARG_MAX - state->params->env_len))
		if (!gsh_next_word(state, NULL))
			return;
}

/*
	*** For our purposes, a "word" is a contiguous sequence of characters
		NOT containing whitespace.
*/
// Init.
//
// Step 0:	[ ] Clean up previous parse state.
// 
// Splitting.
// 
// Step 1:	[x] Split the ENTIRE line into words, and place the addresses
//		of these words in an array.

// Parsing.
// 
// Step 2:	[ ] Using these words, parse tokens such as ';', '$' and '@', 
//		as well as text and/or numbers.

// Expansion.
// 
// Step 3:	[ ] Replace '$' tokens followed by text tokens with the corresponding values.
//		This is also where shell option tokens are processed and removed.

// Creation of command objects.
// 
// Step 4:	[ ] Whenever a ';' token (or newline) is encountered, 
//		create a new command object consisting of the tokens 
//		preceding the semicolon, and which have not yet been placed in a command.
//
//		Push this command object onto the command queue.

// Running.
// 
// Step 5:	[ ] Run and pop the command objects, starting from the first one.
//
void gsh_parse_cmd(struct gsh_parse_state *state,
		   struct gsh_parsed_cmd *cmd)
{
	if (state->lineptr[0] == '\0')
		return;

	// FIXME: WARNING: THIS MIGHT BE WRONG!
	// If the command is a keyword, there won't be a pathname.
	// (At first.)
	
	// Skip any whitespace preceding pathname.
	cmd->pathname = state->lineptr + strspn(state->lineptr, WHITESPACE);

	//if (!gsh_parse_filename(state, params))
	//	return;

	//gsh_parse_cmd_args(state, params);

	// Still more words in line, so start new command.
	//if (*state->word_it)

	cmd->argc = state->word_n;
}
