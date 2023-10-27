#pragma once

#define WHITESPACE " \f\n\r\t\v"

struct gsh_parse_bufs;
struct gsh_parse_state;
struct gsh_params;

// FIXME: (Idea) Replace/merge gsh_parsed_cmd with gsh_parse_bufs?

/*
 *	The maximum number of arguments that can be passed on the command line,
 * 	including the filename.
 */
#define GSH_MAX_ARGS 64

struct gsh_parsed_cmd {
	char *pathname;

	int argc;

	/* List of words to be returned from parsing. */
	char *argv[GSH_MAX_ARGS];
};

void gsh_set_parse_state(struct gsh_parse_state **state,
			 struct gsh_parsed_cmd *parsebufs);

struct gsh_parsed_cmd *gsh_parse_cmd(struct gsh_parse_state *parse_state,
			   const struct gsh_params *params, char *line);