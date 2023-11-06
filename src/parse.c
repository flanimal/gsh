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
#include "lexer.h"
#include "parse.h"
#include "params.h"

#include "special.def.h"

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

void gsh_parse_init(struct gsh_parse_state **parser, struct gsh_params *params)
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
static char *gsh_alloc_wordbuf(struct gsh_expand_state *exp,
			       struct gsh_token *tok, int inc)
{
	const size_t new_len = tok->len + inc;
	const size_t buf_n = exp->buf_n;

	char *newbuf = realloc(exp->bufs[buf_n], new_len + 1);
	if (!exp->bufs[buf_n])
		strcpy(newbuf, tok->data);

	exp->bufs[buf_n] = newbuf;
	tok->data = newbuf;

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
 *		    [------]
 *		      Span
 *
 *	After :	"He1245ld!     "
 *
 *	Example:
 *		Argument is string "ABCDEFGHIJKLMO"
 *		(notice this is larger than buffer)
 *
 *	Before : "Hello, world!"
 *		    [------]
 *		      Span
 *
 *	After :	"HeABCDEFGHIJKLMOld!"
 */
static void gsh_expand_span(struct gsh_expand_state *exp, struct gsh_token *tok,
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
	const ptrdiff_t span_pos = span->begin - tok->data;
	span->begin = gsh_alloc_wordbuf(exp, tok, size_inc) + span_pos;

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
static void gsh_fmt_var(struct gsh_expand_state *exp, struct gsh_token *tok,
			struct gsh_fmt_span *span)
{
	if (strcmp(tok->data, span->begin) == 0) {
		tok->data = (char *)gsh_getenv(exp->params, span->begin + 1);
		return;
	}
	// TODO: Max var name length?
	char *var_name = strndup(span->begin + 1, span->len - 1);

	gsh_expand_span(exp, tok, span, gsh_getenv(exp->params, var_name));
	free(var_name);
}

enum gsh_special_param { GSH_PARAM_STATUS = '?' };

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_expand_state *exp, struct gsh_token *tok,
			  char *const fmt_begin)
{
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1, gsh_special_chars) + 1,
	};

	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_PARAM_STATUS:
		span.fmt_str = "%d";

		gsh_expand_span(exp, tok, &span, exp->params->last_status);
		break;
	default:
		span.fmt_str = "%s";

		gsh_fmt_var(exp, tok, &span);
		break;
	}
}

/*	Substitute the home character with the value of $HOME.
*
	If the word consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_expand_state *exp, struct gsh_token *tok,
			 char *const fmt_begin)
{
	const char *homevar = gsh_getenv(exp->params, "HOME");

	// TODO: Move the whole-word check out of the fmt_* functions so that
	// it makes more sense with strpbrk() converting const char * to char *?
	if (strcmp(tok->data, (char[]){ GSH_REF_HOME, '\0' }) == 0) {
		tok->data = (char *)homevar;
		return;
	}

	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s",
	};

	gsh_expand_span(exp, tok, &span, homevar);
}

static void gsh_free_parsed(struct gsh_parse_state *p)
{
	p->expand_st->skip = 0;

	// Delete substitution buffers.
	for (; p->expand_st->buf_n > 0; --p->expand_st->buf_n) {
		free(p->expand_st->bufs[p->expand_st->buf_n - 1]);
		p->expand_st->bufs[p->expand_st->buf_n - 1] = NULL;
	}

	// Remember that in order to free a linked list, you need
	// two pointers-- one to hold the element to delete,
	// and another to hold the next element.

	// Delete tokens and command objects.
	for (struct gsh_token *tok_it = LIST_FIRST(&p->tok_front), *next;
	     tok_it; tok_it = next) {
		next = LIST_NEXT(tok_it, entry);
		// test
		LIST_REMOVE(tok_it, entry);
		free(tok_it);
	}
}

static void gsh_expand(struct gsh_expand_state *exp, struct gsh_token *tok)
{
	for (char *fmt_begin = tok->data;
	     (fmt_begin = strpbrk(fmt_begin, "$~"));) {
		switch ((enum gsh_token_type)fmt_begin[0]) {
		case GSH_REF_PARAM:
			gsh_fmt_param(exp, tok, fmt_begin);
			continue;
		case GSH_REF_HOME:
			gsh_fmt_home(exp, tok, fmt_begin);
			continue;
		default:
			unreachable();
		}
	}
}

/*
	Iterate tokens, starting from a quote and ending at
	a quote of the same time.

	Create an argument using all text between the two quotes,
	exclusive.
*/
static char *gsh_parse_quoted(struct gsh_parse_state *p,
			      struct gsh_token **tok_it,
			      enum gsh_token_type quote_type)
{
	size_t arg_len = 0;

	char *arg = (*tok_it)->data + 1;

	while ((*tok_it = LIST_NEXT(*tok_it, entry)) &&
	       ((*tok_it)->type != quote_type)) {
		// Reallocate the argument.
		const size_t buf_n = p->expand_st->buf_n;

		char *newbuf = realloc(p->expand_st->bufs[buf_n],
				       arg_len + (*tok_it)->len + 1);
		if (!p->expand_st->bufs[buf_n])
			*stpncpy(newbuf, arg, arg_len) = '\0';

		p->expand_st->bufs[buf_n] = newbuf;
		arg = newbuf;

		arg_len += (*tok_it)->len;
	}

	return arg;
}

static void gsh_push_cmd(struct gsh_parsed_cmd **cmd)
{
	(*cmd)->pathname = (*cmd)->argv[0];

	char *last_slash = strrchr((*cmd)->argv[0], '/');
	if (last_slash)
		(*cmd)->argv[0] = last_slash + 1;
}

enum gsh_node_type {
	GSH_NODE_LINE,
	GSH_NODE_CMD,
	GSH_NODE_ARG,
	GSH_NODE_REF,
	GSH_NODE_TOK,
};

struct gsh_syntax_elem {
	char *data;
	size_t len;
};

struct gsh_parse_node {
	struct gsh_parse_node *parent;

	enum gsh_node_type type;

	union {
		struct gsh_syntax_elem elem;
		struct gsh_token tok;
	};

	size_t desc_n;
	struct gsh_parse_node **desc;
};

static struct gsh_parse_node *gsh_new_node(enum gsh_node_type type,
					   size_t desc_n)
{
	struct gsh_parse_node *node =
		malloc(sizeof(*node) + sizeof(node) * desc_n);

	node->type = type;
	node->desc_n = desc_n;

	return node;
}

/*
*	There are currently two reasons that we output whitespace tokens:
* 
*	1.	To make separation of arguments possible, since they can consist
*		of multiple word and ref tokens.
* 
*			- This _could_ also be done by checking if the location of the
*			  next token immediately follows the last token: if not, there is
*			  whitespace in between, and it is a new argument.
* 
*	2.	To output whitespace occurring within quotes that we don't want
*		to remove.
* 
*			- If the lexer identified strings instead of the parser,
//			  it would remove this need.
//		
//			  Of course, this raises the question of how substitutions 
//			  within quotes would work.
*/

/*
	1. Two string classes: `string` and `subst-string`
	2. A normal `string` class in addition to `string-beg` and `string-end`
		for strings with substitutions

	Take C++ raw string as an example

	*** I don't think the lexer should perform expansions.
	
	Example of the problem:

	Line : hello$SHELL
	vs.
	Line : hello $SHELL

	A rule that says that two neighboring word tokens are to be
	concatenated won't solve this problem.

	*** If the lexer can identify strings, I don't think identifying references
	would be unreasonable.

	*** What if we somehow _grouped_ contigious tokens together instead of explicitly
	*** emitting whitespace tokens?
	
*/

/* 
	1. Initial lexing
	2. Expansion of references
	3. Pass to parser
*/

// The first parsing phase gets each token from the lexer, and creates a new
// node for the token underneath a `Line` node.

// Is there any reason to get tokens one at a time instead of all at once?
//
//	- Getting tokens one at a time means we can store them in nodes
// immediately 	  without allocating a queue to push them to first.

//	- Getting the tokens all at once means we know how many there are,
//	  and can allocate memory accordingly.
//		- However, this would require allocating twice for each token,
//		  unless the lexer output an array of `gsh_parse_node`.

//	So if we need to allocate anyway, should we just get them all at once?
//
void gsh_parse_cmd(struct gsh_parse_state *p)
{
	gsh_free_parsed(p);

	struct gsh_lexer_state *lex = gsh_new_lexer_state();
	struct gsh_parse_node *root = gsh_new_node(GSH_NODE_LINE, 64);

	size_t n = 0;

	for (struct gsh_token *tok;
	     (tok = gsh_get_token(lex)) && n < root->desc_n;) {
		(root->desc[n++] = gsh_new_node(GSH_NODE_TOK, 0))->data =
			tok->data;
	}
}

static void gsh_parse_first()
{
}

static void gsh_parse_second()
{
}

static void gsh_parse_third()
{
}