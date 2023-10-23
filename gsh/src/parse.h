#pragma once

struct gsh_parse_bufs;
struct gsh_parse_state;
struct gsh_params;

void gsh_set_parse_state(struct gsh_parse_bufs *parsebufs,
			 struct gsh_parse_state **state);

char **gsh_parse_cmd(struct gsh_params *params,
		     struct gsh_parse_state *parse_state, char **line);