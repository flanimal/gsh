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
	LIST_ENTRY(gsh_token) entry;

	char *data;
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
	LIST_HEAD(tok_queue, gsh_token) tok_front;
	LIST_HEAD(cmd_queue, gsh_parsed_cmd) cmd_front;
};

struct gsh_parsed_cmd {
	LIST_ENTRY(gsh_parsed_cmd) entry;

	char *pathname;

	int argc;

	char *argv[64];
};

void gsh_parse_init(struct gsh_parser **parser, struct gsh_params *params);

void gsh_split_words(struct gsh_parser *p, char *line, size_t max_size);

void gsh_parse_cmd(struct gsh_parser *p);