#pragma once

#include <sys/queue.h>

#include <stddef.h>

#include "../src/special.def"

#define WHITESPACE " \f\n\r\t\v"

// The previous approach was:
// 
// Tokenization --(Token stream)-> Parsing/Expansion 
//	-> (Command objects)
// 
//	Here, the "parser" gets tokens to first expand them, then generate 
//	the resulting arguments, then create command objects from 
//	those arguments: _3_ jobs wrapped into one.
// 
// A better approach might be:
// 
// Tokenization --(Token stream)-> Parsing --(AST)-> Argument generation 
//	-> (Command objects)
//
// where the parser has only the single responsibility of generating
// an AST. Expansion happens during argument generation, which itself 
// is separate from creation of command objects.

/*	The maximum number of words to accept before reallocating the 
 *	word list.
 */
#define GSH_MIN_WORD_N 64

struct gsh_params;

struct gsh_parse_state {
	struct gsh_expand_state *expand_st;

	LIST_HEAD(cmd_queue, gsh_parsed_cmd) cmd_front;
};

struct gsh_parsed_cmd {
	LIST_ENTRY(gsh_parsed_cmd) entry;

	char *pathname;

	int argc;
	char *argv[64];
};

void gsh_parse_init(struct gsh_parse_state **parser, struct gsh_params *params);

void gsh_split_words(struct gsh_parse_state *p, char *line, size_t max_size);

void gsh_parse_cmd(struct gsh_parse_state *p);