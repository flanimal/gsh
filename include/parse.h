#pragma once

#define WHITESPACE " \f\n\r\t\v"

struct gsh_parse_bufs;
struct gsh_parse_state;
struct gsh_params;

void gsh_set_parse_state(struct gsh_parse_state **state,
			 struct gsh_parse_bufs *parsebufs);

char *const *gsh_parse_cmd(struct gsh_parse_state *parse_state,
			   const struct gsh_params *params, char **line);