#include <limits.h>
#include <search.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

// FIXME: Remove this include.
#include "gsh.h"
#include "parse.h"
#include "params.h"

#include "special.def"

#if defined(__GNUC__)
#define unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
#define unreachable() __assume(0)
#endif

void gsh_set_opt(struct gsh_state *sh, char *name, bool value);

struct gsh_parser {
	struct gsh_expand_state *expand_st;

	/* Pointer to the next word in the line, gotten by strtok_r(). */
	char *lineptr;

	/* Iterator pointing to current word. */
	const char **word_it;

	/* Must be below ARG_MAX/__POSIX_ARG_MAX. */
	size_t words_size;
	size_t word_n;

	char *words[];
};

// TODO: Should this be the state for a single expansion, or all so far?
struct gsh_expand_state {
	const struct gsh_params *params;

	/* Position within word to begin expansion. */
	size_t skip;

	size_t size_inc;

	/* Stack of buffers for words that contain substitutions. */
	size_t buf_n;
	char *bufs[];
};

struct gsh_fmt_span {
	/* Beginning of format span within the current word. */
	char *begin;

	/* Length of the unformatted span, up to either the zero byte or the
	 * next special char. */
	size_t len;

	const char *fmt_str;
};

struct gsh_parsed_cmd *gsh_new_cmd(struct gsh_cmd_queue *queue)
{
	struct gsh_parsed_cmd *cmd = calloc(1, sizeof(*cmd));
	insque(cmd, queue->front);

	return cmd;
}

void gsh_parse_init(struct gsh_parser **parser, struct gsh_params *params)
{
	const size_t words_size = GSH_MIN_WORD_N * sizeof(char *);

	*parser = calloc(1, sizeof(**parser) + words_size);
	(*parser)->word_it = (const char **)(*parser)->words;

	(*parser)->expand_st =
		calloc(1, sizeof(*(*parser)->expand_st) + words_size);
	(*parser)->expand_st->params = params;
}

/*	Allocate and return a word buffer.
 */
static char *gsh_alloc_wordbuf(struct gsh_expand_state *exp, char **word_it,
			       int inc)
{
	const size_t new_len = strlen(*word_it) + inc;
	const size_t buf_n = exp->buf_n;

	char *newbuf = realloc(exp->bufs[buf_n], new_len + 1);
	if (!exp->bufs[buf_n])
		strcpy(newbuf, *word_it);

	exp->bufs[buf_n] = newbuf;
	*word_it = newbuf;

	return newbuf;
}

/*	Format a span within a word with the given args, allocating a buffer if
 *	necessary. Returns the increase in size if there is one.
 */
static void gsh_expand_span(struct gsh_expand_state *exp,
			    const char **word_it, struct gsh_fmt_span *span,
			    ...)
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
		exp->skip += vsprintf(span->begin, span->fmt_str, fmt_args);

		va_end(fmt_args);
		return;
	}

	exp->size_inc += size_inc;

	const ptrdiff_t before_len = span->begin - *word_it;

	// TODO: (Idea) Allocate and fill with empty space instead of duping
	// after? Need to allocate.
	char *after = strdup(span->begin + span->len);
	span->begin = gsh_alloc_wordbuf(exp, word_it, size_inc) + before_len;

	vsprintf(span->begin, span->fmt_str, fmt_args);
	strcpy(span->begin + print_len, after);

	exp->skip += (span->begin) ? (size_t)print_len : span->len;

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
static void gsh_fmt_var(struct gsh_expand_state *exp, const char **word_it,
			struct gsh_fmt_span *span)
{
	if (strcmp(*word_it, span->begin) == 0) {
		*word_it = gsh_getenv(exp->params, span->begin + 1);
		return;
	}

	char *var_name = strndup(span->begin + 1, span->len - 1);

	gsh_expand_span(exp, word_it, span,
			gsh_getenv(exp->params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_expand_state *exp, const char **word_it,
			  char *const fmt_begin)
{
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1, gsh_special_chars) + 1,
	};

	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_CHAR_PARAM_STATUS:
		span.fmt_str = "%d";

		gsh_expand_span(exp, word_it, &span,
				exp->params->last_status);
		break;
	default:
		span.fmt_str = "%s";

		gsh_fmt_var(exp, word_it, &span);
		break;
	}
}

/*	Substitute the home character with the value of $HOME.
*
	If the word consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_expand_state *exp, const char **word_it,
			 char *const fmt_begin)
{
	const char *homevar = gsh_getenv(exp->params, "HOME");

	// TODO: Move the whole-word check out of the fmt_* functions so that
	// it makes more sense with strpbrk() converting const char * to char *?
	if (strcmp(*word_it, (char[]){ GSH_CHAR_HOME, '\0' }) == 0) {
		*word_it = homevar;
		return;
	}

	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s",
	};

	gsh_expand_span(exp, &span, homevar);
}

/*      Expand the last word.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_word(struct gsh_expand_state *exp,
			    const char **word_it)
{
	char *fmt_begin = strpbrk(*word_it + exp->skip, gsh_special_chars);

	if (!fmt_begin)
		return false;

	switch ((enum gsh_special_char)fmt_begin[0]) {
	case GSH_CHAR_PARAM:
		gsh_fmt_param(exp, word_it, fmt_begin);
		return true;
	case GSH_CHAR_HOME:
		gsh_fmt_home(exp, word_it, fmt_begin);
		return true;
	}

	unreachable();
}

/*      Collect and insert a fully-expanded word into the list.
 *
 *	Returns next word or NULL if no next word, similar to strtok().
 */
static const char *gsh_next_word(struct gsh_parser *p, char *line)
{
	*p->word_it = strtok_r(line, WHITESPACE, &p->lineptr);
	if (!(*p->word_it))
		return NULL;

	const size_t old_words_size = p->words_size;

	// FIXME: ... is using lineptr a good idea?
	p->words_size += p->lineptr - *p->word_it;

	while (gsh_expand_word(p->expand_st, *p->word_it))
		;

	p->words_size += p->expand_st->size_inc;
	const size_t word_len = p->words_size - old_words_size;

	if (*p->word_it[word_len - 1] == ';')
		// End current command, and begin another with remaining
		// words.
		//
		// This could be done by checking that we have reached
		// the end of the input buffer; if not, it must mean
		// there was more than one command.
		return;

	p->expand_st->skip = 0;
	if (p->expand_st->bufs[p->expand_st->buf_n])
		++p->expand_st->buf_n;

	++p->word_n;
	return *p->word_it++;
}

/*      Parse the first word in the input line, and place
 *      the filename in the argument array.
 */
 static bool gsh_parse_filename(struct gsh_parser *p)
{
	if (!p->words[0])
		return false;

	char *last_slash = strrchr(p->words[0], '/');
	if (last_slash)
		p->words[0] = last_slash + 1;

	return !!p->words[0];
 }

 // FIXME:
/*	Parse words and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 */
 static void gsh_parse_cmd_args(struct gsh_parser *p)
 {
	while (p->words_size <= (_POSIX_ARG_MAX - p->expand_st->params->env_len))
		if (!gsh_next_word(p, NULL))
			return;
}

static void gsh_free_parsed(struct gsh_parser *p)
{
	p->expand_st->skip = 0;

	// Delete substitution buffers.
	for (; p->expand_st->buf_n > 0; --p->expand_st->buf_n) {
		free(p->expand_st->bufs[p->expand_st->buf_n - 1]);
		p->expand_st->bufs[p->expand_st->buf_n - 1] = NULL;
	}

	// Reset word list.
	for (; p->word_n > 0; --p->word_n)
		*(--p->word_it) = NULL;
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
			// gsh_set_opt(sh, shopt_ch + 1, val);
			//  FIXME: Push shopt command onto queue.
		}
	}

	if (!after) {
		*shopt_ch = '\0';
		return;
	}

	while (shopt_ch != after + 1)
		*shopt_ch++ = ' ';
}

void gsh_split_words(struct gsh_parser *p, char *line, size_t max_size)
{
	gsh_free_parsed(p);
	p->lineptr = line;

	while (p->words_size <= max_size)
		if (!gsh_next_word(p, NULL))
			return;
}

void gsh_parse_cmd(struct gsh_parser *p, struct gsh_cmd_queue *cmd_queue)
{
	if (p->lineptr[0] == '\0')
		return;

	// FIXME: WARNING: THIS MIGHT BE WRONG!
	// If the command is a keyword, there won't be a pathname.
	// (At first.)

	struct gsh_parsed_cmd *cmd = gsh_new_cmd(cmd_queue);

	// Skip any whitespace preceding pathname.
	cmd->pathname = p->lineptr + strspn(p->lineptr, WHITESPACE);

	 if (!gsh_parse_filename(p))
		return;

	 gsh_parse_cmd_args(p);

	// Still more words in line, so start new command.
	// if (*state->word_it)

	cmd->argc = p->word_n;
}
