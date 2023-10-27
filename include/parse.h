#pragma once

#define WHITESPACE " \f\n\r\t\v"

struct gsh_parse_bufs;
struct gsh_parse_state;
struct gsh_params;

struct gsh_parsed_cmd {
	char *pathname;
	char *const *argv;
};

void gsh_set_parse_state(struct gsh_parse_state **state,
			 struct gsh_parse_bufs *parsebufs);

struct gsh_parsed_cmd gsh_parse_cmd(struct gsh_parse_state *parse_state,
			   const struct gsh_params *params, char *line);