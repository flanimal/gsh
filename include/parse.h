#pragma once

#include <stddef.h>

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

struct gsh_parsed_cmd
{
	struct gsh_parsed_cmd *back, *forw;

	char *pathname;

	int argc;

	/* Pointer to beginning of this command's arguments in the word list. */
	char **argv;
};

void gsh_parse_init(struct gsh_parser **parser, struct gsh_params *params);

void gsh_split_words(struct gsh_parser *p, char *line, size_t max_size);

void gsh_parse_cmd(struct gsh_parser *parser,
		   struct gsh_cmd_queue *cmd_queue);