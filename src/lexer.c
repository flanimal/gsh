#include <limits.h>
#include <search.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#include "lexer.h"

struct gsh_lexer_state {
	char *line_it;

	/* Must be below ARG_MAX/__POSIX_ARG_MAX. */
	size_t tokens_size;

	// FIXME: Replace with singly-linked list queues
	// if possible.
	/* Token queue. */
	LIST_HEAD(tok_queue, gsh_token) tok_front;
};

struct gsh_lexer_state* gsh_new_lexer_state()
{
	return malloc(sizeof(struct gsh_lexer_state));
}

static struct gsh_token *gsh_new_tok()
{
	return malloc(sizeof(struct gsh_token));
}

struct gsh_token *gsh_get_token(struct gsh_lexer_state *lex)
{
	if (*lex->line_it == '\0')
		return NULL;

	struct gsh_token *tok = gsh_new_tok();

	const size_t word_len = strcspn(lex->line_it, gsh_special_chars);

	if (word_len == 0) {
		tok->type = (enum gsh_token_type)(*lex->line_it);
		
		tok->tok_ch = *lex->line_it++;
		++lex->tokens_size;
	} else {
		tok->type = GSH_WORD;

		tok->tok_text.data = lex->line_it;

		lex->line_it += word_len;
		lex->tokens_size += word_len;

		tok->tok_text.len = word_len;
	}

	// FIXME: Head or not????
	LIST_INSERT_HEAD(&lex->tok_front, tok, entry);
	return tok;
}