#include <sys/queue.h>

#include <stddef.h>

#include "special.def.h"

struct gsh_token_data
{
	char *data;
	size_t len;
};

struct gsh_token {
	LIST_ENTRY(gsh_token) entry;

	enum gsh_token_type type;

	union {
		struct gsh_token_data tok_text;
		char tok_ch;
	};
};

struct gsh_lexer_state;

struct gsh_lexer_state *gsh_new_lexer_state();

struct gsh_token *gsh_get_token(struct gsh_lexer_state *p);