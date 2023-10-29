#pragma once

#include <stddef.h>

#define WHITESPACE " \f\n\r\t\v"

/*	The maximum number of words to accept before reallocating the 
 *	word list.
 */
#define GSH_MIN_WORD_N 64

// FIXME: Do we need a maximum number of commands per line?

/*
*	gsh_words		: Array of word locations in input line.
*	gsh_parsed_cmd		: A stack of one or more parsed commands.
* 
*	gsh_parse_state		: State used for parsing/tokenizing(?)
*	gsh_expand_state	: State used for expansions following parsing/tokenizing(?).
*/

struct gsh_parse_state;
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

void gsh_split_words(struct gsh_parse_state *state, char *line);

void gsh_parse_cmd(struct gsh_parse_state *state,
		   struct gsh_cmd_queue *cmd_queue);