#pragma once

#define WHITESPACE " \f\n\r\t\v"

/*	The maximum number of arguments to accept before reallocating the 
 *	argument list.
 */
#define GSH_MIN_ARG_N 64

struct gsh_parse_state;
struct gsh_params;

struct gsh_words {

};

struct gsh_parsed_cmd {
	char *pathname;

	int argc;
	char *argv[];
};

void gsh_parse_init(struct gsh_parse_state **state,
		    struct gsh_parsed_cmd **parsebufs, struct gsh_params *params);

void gsh_split_words(struct gsh_parse_state *state, char *line);

void gsh_parse_cmd(struct gsh_parse_state *state,
		   struct gsh_parsed_cmd *cmd);