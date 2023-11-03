#include <sys/queue.h>

#include <stddef.h>

#include "special.def.h"

struct gsh_token {
	LIST_ENTRY(gsh_token) entry;

	char *data;
	size_t len;

	enum gsh_token_type type;
};

struct gsh_lexer_state;

struct gsh_lexer_state *gsh_new_lexer_state();

struct gsh_token *gsh_get_token(struct gsh_lexer_state *p);