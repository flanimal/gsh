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

#if defined(__GNUC__)
#define unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
#define unreachable() __assume(0)
#endif

void gsh_set_opt(struct gsh_state *sh, char *name, bool value);

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

struct gsh_parsed_cmd *gsh_new_cmd()
{
	return calloc(1, sizeof(struct gsh_parsed_cmd));
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
 *
 *	Automatically reallocates if the buffer is not large enough,
 *	and automatically shifts the rest of the string down when
 *	there would be empty space.
 *
 *	Example:
 *		Argument is integer 1245.
 *
 *	Before : "Hello, world!"
 *		    {------}
 *		      Span
 *
 *	After :	"He1245ld!     "
 *
 *	Example:
 *		Argument is string "ABCDEFGHIJKLMO"
 *		(notice this is larger than buffer)
 *
 *	Before : "Hello, world!"
 *		    {------}
 *		      Span
 *
 *	After :	"HeABCDEFGHIJKLMOld!"
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
	// TODO: Max var name length?
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

/*      Parse the first word in the input line, and place
 *      the filename in the argument array.
 */
// static bool gsh_parse_filename(struct gsh_parser *p)
//{
//	if (!p->tokens[0])
//		return false;
//
//	char *last_slash = strrchr(p->tokens[0], '/');
//	if (last_slash)
//		p->tokens[0] = last_slash + 1;
//
//	return !!p->tokens[0];
// }

static void gsh_free_parsed(struct gsh_parser *p)
{
	p->expand_st->skip = 0;

	// Delete substitution buffers.
	for (; p->expand_st->buf_n > 0; --p->expand_st->buf_n) {
		free(p->expand_st->bufs[p->expand_st->buf_n - 1]);
		p->expand_st->bufs[p->expand_st->buf_n - 1] = NULL;
	}
}

/*	Pop a token from the queue and store in `out_pop`.
 *
 *	Returns true if there was a token to pop.
 *
 *	If the last token has been popped, sets the queue pointer to NULL.
 */
static bool gsh_pop_tok(struct tok_queue *front, struct gsh_token *to_pop)
{
	LIST_REMOVE(to_pop, entry);
	free(to_pop);
}

static void gsh_expand(struct gsh_expand_state *exp, const char **tok_data)
{
	while (1) {
		char *exp_pos = strchr(*tok_data, "$");
		if (exp_pos) {
			gsh_fmt_param(exp, tok_data, &exp_pos);
			continue;
		}

		exp_pos = strchr(*tok_data, "~");

		if (exp_pos) {
			gsh_fmt_home(exp, tok_data, &exp_pos);
			continue;
		}

		return;
	}
}

/*
	Iterate tokens, starting from a quote and ending at
	a quote of the same time.

	Create an argument using all text between the two quotes,
	exclusive.
*/
static char *gsh_parse_quoted(struct gsh_parser *p, struct gsh_token *begin,
			      enum gsh_special_char quote_type)
{
	size_t arg_len = 0;

	// Arg begins at text immediately following quote.
	char *arg = LIST_NEXT(begin, entry)->data;

	struct gsh_token *popped;
	LIST_FOREACH(popped, p->front, entry)
	{
		if (popped->type != quote_type) {
			// Reallocate the argument.
			const size_t buf_n = p->expand_st->buf_n;

			char *newbuf = realloc(p->expand_st->bufs[buf_n],
					       arg_len + popped->len + 1);
			if (!p->expand_st->bufs[buf_n])
				*stpncpy(newbuf, arg, arg_len) = '\0';

			p->expand_st->bufs[buf_n] = newbuf;
			arg = newbuf;

			arg_len += popped->len;
		}
	}

	return arg;
}

/*
 */
static struct gsh_token *gsh_new_tok()
{
	return malloc(sizeof(struct gsh_token));
}

static struct gsh_token *gsh_get_token(struct gsh_parser *p)
{
	if (*p->line_it == '\0')
		return NULL;

	struct gsh_token *tok = gsh_new_tok();

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

	return tok;
}

void gsh_parse_cmd(struct gsh_parser *p, struct cmd_queue *cmd_queue)
{
	// Get tokens (words, quotes, parentheses, etc.).
	// NOTE: For now, we are keeping tokenization and parsing
	// phases independent for simplicity.

	// Get first token. There must be at least one.
	struct gsh_token *prev = gsh_get_token(p);
	LIST_INSERT_HEAD(p->front, prev, entry);

	for (struct gsh_token *tok; (tok = gsh_get_token(p));) {
		LIST_INSERT_AFTER(prev, tok, entry);
		prev = tok;
	}

	// I guess that here, we use "parse" to mean "combining/replacing tokens
	// in the token queue".

	// The first command to insert. There must
	// be at least one command.
	struct gsh_parsed_cmd *prev_cmd = gsh_new_cmd();
	LIST_INSERT_HEAD(cmd_queue, prev_cmd, entry);

	struct gsh_parsed_cmd *cmd;

	struct gsh_token *tok_it;
	LIST_FOREACH(tok_it, p->front, entry)
	{
		switch (tok_it->type) {
		case GSH_WORD:
			gsh_expand(p->expand_st, &tok_it->data);
			cmd->argv[cmd->argc++] = tok_it->data;

			break;
		case GSH_CHAR_SINGLE_QUOTE:
		case GSH_CHAR_DOUBLE_QUOTE:
			cmd->argv[cmd->argc++] = gsh_parse_quoted(
				p, tok_it, tok_it->type);

			break;
		case GSH_CHAR_CMD_SEP:
			LIST_INSERT_AFTER(prev_cmd, cmd, entry);
			cmd = gsh_new_cmd();

			break;
		}
	}

	// Reached end of line.
	// gsh_push_cmd(cmd_queue);
	LIST_INSERT_AFTER(prev_cmd, cmd, entry);

	// TODO: Increment tokens_size AFTER expansions have been performed.
}
