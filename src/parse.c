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

struct gsh_token {
	struct gsh_token *next, *prev;

	const char *data;
	size_t len;

	enum gsh_special_char type;
};

struct gsh_parser {
	struct gsh_expand_state *expand_st;

	/* Pointer to the next token in the line. */
	char *line_it;

	// TODO: Use size stored in token structs instead?
	/* Must be below ARG_MAX/__POSIX_ARG_MAX. */
	size_t tokens_size;

	/* Token queue. */
	struct gsh_token *tokens;
};

// NOTE:	Should this be the state for a single expansion, or single word,
//		or should it be used for all?
struct gsh_expand_state {
	const struct gsh_params *params;

	/* The length within the word to skip. */
	size_t skip;
	/* Increase in word size after expansions. */
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

	// TODO: It may be unwise to initialize expand_state::bufs with
	// calloc().
	(*parser)->expand_st =
		calloc(1, sizeof(*(*parser)->expand_st) + words_size);
	(*parser)->expand_st->params = params;
}

/*	Allocate and return a word buffer.
 */
static char *gsh_alloc_wordbuf(struct gsh_expand_state *exp, const char **word,
			       int inc)
{
	// FIXME: May want to get rid of strlen().
	const size_t new_len = strlen(*word) + inc;
	const size_t buf_n = exp->buf_n;

	char *newbuf = realloc(exp->bufs[buf_n], new_len + 1);
	if (!exp->bufs[buf_n])
		strcpy(newbuf, *word);

	exp->bufs[buf_n] = newbuf;
	*word = newbuf;

	return newbuf;
}

/*	Format a span within a word with the given args.
 */
static void gsh_expand_span(struct gsh_expand_state *exp, const char **word,
			    struct gsh_fmt_span *span, ...)
{
	va_list fmt_args;

	int print_len;
	{
		const char nul_rep = span->begin[span->len];

		va_start(fmt_args, span);
		print_len = vsnprintf(span->begin, span->len, span->fmt_str,
				      fmt_args);

		assert(print_len >= 0);
		va_end(fmt_args);

		span->begin[span->len] = nul_rep;
	}

	exp->skip += print_len;

	const int size_inc = print_len - span->len;
	if (size_inc == 0)
		return;

	exp->size_inc += size_inc;

	if (size_inc < 0) {
		memmove(span->begin + print_len, span->begin + span->len,
			span->len);
		return;
	}

	// Need to allocate.
	const ptrdiff_t span_pos = span->begin - *word;
	span->begin = gsh_alloc_wordbuf(exp, word, size_inc) + span_pos;

	memmove(span->begin + print_len, span->begin + span->len, span->len);

	va_start(fmt_args, span);
	vsprintf(span->begin, span->fmt_str, fmt_args);
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
static void gsh_fmt_var(struct gsh_expand_state *exp, const char **word,
			struct gsh_fmt_span *span)
{
	if (strcmp(*word, span->begin) == 0) {
		*word = gsh_getenv(exp->params, span->begin + 1);
		return;
	}

	char *var_name = strndup(span->begin + 1, span->len - 1);

	gsh_expand_span(exp, word, span, gsh_getenv(exp->params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_expand_state *exp, const char **word,
			  char *const fmt_begin)
{
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1, gsh_special_chars) + 1,
	};

	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_CHAR_PARAM_STATUS:
		span.fmt_str = "%d";

		gsh_expand_span(exp, word, &span, exp->params->last_status);
		break;
	default:
		span.fmt_str = "%s";

		gsh_fmt_var(exp, word, &span);
		break;
	}
}

/*	Substitute the home character with the value of $HOME.
*
	If the word consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_expand_state *exp, const char **word,
			 char *const fmt_begin)
{
	const char *homevar = gsh_getenv(exp->params, "HOME");

	// TODO: Move the whole-word check out of the fmt_* functions so that
	// it makes more sense with strpbrk() converting const char * to char *?
	if (strcmp(*word, (char[]){ GSH_CHAR_HOME, '\0' }) == 0) {
		*word = homevar;
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
static bool gsh_expand_word(struct gsh_expand_state *exp, const char **word)
{
	char *fmt_begin = strpbrk(*word + exp->skip, gsh_special_chars);

	if (!fmt_begin)
		return false;

	switch ((enum gsh_special_char)fmt_begin[0]) {
	case GSH_CHAR_PARAM:
		gsh_fmt_param(exp, word, fmt_begin);
		return true;
	case GSH_CHAR_HOME:
		gsh_fmt_home(exp, word, fmt_begin);
		return true;
	}

	unreachable();
}

/*      Collect and insert a into the list.
 *
 *	Returns next word or NULL if no next word, similar to strtok().
 */
static const char *gsh_next_word(struct gsh_parser *p, char *line)
{
	char *word = strtok_r(line, WHITESPACE, &p->line_it);
	if (!word)
		return NULL;

	const size_t old_words_size = p->tokens_size;

	// FIXME: ... is using lineptr a good idea?
	p->tokens_size += p->line_it - word;

	p->tokens_size += p->expand_st->size_inc;
	const size_t word_len = p->tokens_size - old_words_size;

	p->expand_st->skip = 0;
	if (p->expand_st->bufs[p->expand_st->buf_n])
		++p->expand_st->buf_n;

	return (p->tokens[p->token_n++] = word);
}

/*      Parse the first word in the input line, and place
 *      the filename in the argument array.
 */
static bool gsh_parse_filename(struct gsh_parser *p)
{
	if (!p->tokens[0])
		return false;

	char *last_slash = strrchr(p->tokens[0], '/');
	if (last_slash)
		p->tokens[0] = last_slash + 1;

	return !!p->tokens[0];
}

static void gsh_free_parsed(struct gsh_parser *p)
{
	p->expand_st->skip = 0;

	// Delete substitution buffers.
	for (; p->expand_st->buf_n > 0; --p->expand_st->buf_n) {
		free(p->expand_st->bufs[p->expand_st->buf_n - 1]);
		p->expand_st->bufs[p->expand_st->buf_n - 1] = NULL;
	}
}

static struct gsh_token *gsh_new_tok(struct gsh_token **last)
{
	struct gsh_token *tok = malloc(sizeof(*tok));

	if (!(*last))
		*last = tok;

	insque(tok, *last);
	return tok;
}

static bool gsh_pop_tok(struct gsh_token **last, struct gsh_token *out_pop)
{
	if (*last) {
		out_pop = *last;

		if (!(*last)->prev)
			*last = NULL;

		remque(*last);
		free(*pop);

		return true;
	}

	return false;
}

// Iterate over each character in the input line individually
// (until we have a reason to get more at a time).
static bool gsh_get_token(struct gsh_parser *p)
{
	if (*p->line_it == '\0')
		return false;

	struct gsh_token *tok = gsh_new_tok(p->tokens);

	const size_t word_len = strcspn(p->line_it, gsh_special_chars);

	if (word_len == 0) {
		tok->type = (enum gsh_special_char)(*p->line_it);
		tok->len = 1;
	} else {
		tok->type = GSH_WORD;
		tok->len = word_len;
	}

	tok->data = p->line_it;
	p->line_it += tok->len;

	return true;
}

void gsh_parse_cmd(struct gsh_parser *p, struct gsh_cmd_queue *cmd_queue)
{
	// Tokenize.
	while (gsh_get_token(p))
		;

	// TODO: Increment tokens_size AFTER expansions have been performed.

	// Pop tokens to create command objects, and push those commands onto
	// cmd_queue.
	//
	// To elaborate, the final argv list will be similar to the initial
	// token list. Words will remain unmodified. However, parameter
	// references and such will be substituted with a single word token each
	// containing the referenced value.

	for (struct gsh_token tok; gsh_pop_tok(&p->tokens, &tok)) {
		switch () {
		}
	}
}
