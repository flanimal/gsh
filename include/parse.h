#pragma once

#define WHITESPACE " \f\n\r\t\v"

/*
 *	The maximum number of arguments that can be passed on the command line,
 * 	including the filename.
 */
#define GSH_MAX_ARGS 64

struct gsh_parse_state;
struct gsh_params;

struct gsh_parsed_cmd {
	char *pathname;

	int argc;

	/* List of words to be returned from parsing. */
	char *argv[GSH_MAX_ARGS];
};

void gsh_parse_init(struct gsh_parse_state **state,
		    struct gsh_parsed_cmd **parsebufs);

void gsh_parse_cmd(struct gsh_parsed_cmd *cmd, struct gsh_parse_state *state,
		   struct gsh_params *params);