#pragma once

#include <sys/queue.h>

#include <stddef.h>

#include "../src/special.def"

#define WHITESPACE " \f\n\r\t\v"

/*	The maximum number of words to accept before reallocating the 
 *	word list.
 */
#define GSH_MIN_WORD_N 64

/*
 *	gsh_parser		: State, buffers used for parsing/tokenizing(?)
 *	gsh_parsed_cmd		: A stack of one or more parsed commands.
 * 
 *	gsh_expand_state	: State used for expansions following parsing/tokenizing(?).
 */

struct gsh_params;

struct gsh_token {
	SLIST_ENTRY(gsh_token) next;

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
	SLIST_HEAD(tok_queue, gsh_token) *front;
};

struct gsh_parsed_cmd {
	SLIST_ENTRY(gsh_parsed_cmd) next;

	char *pathname;

	int argc;

	/* Pointer to beginning of this command's arguments in the token list.
	 */
	char **argv;
};

SLIST_HEAD(cmd_queue, gsh_parsed_cmd);

void gsh_parse_init(struct gsh_parser **parser, struct gsh_params *params);

void gsh_split_words(struct gsh_parser *p, char *line, size_t max_size);

void gsh_parse_cmd(struct gsh_parser *parser,
		   struct gsh_cmd_queue *cmd_queue);