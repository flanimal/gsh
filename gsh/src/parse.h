#pragma once

struct gsh_parse_bufs *gsh_init_parsebufs();

size_t gsh_max_input(const struct gsh_state *sh);

/*	Get a zero-terminated line of input from the terminal,
 *	excluding the newline.
 */
bool gsh_read_line(struct gsh_input_buf *input);

void gsh_parse_and_run(struct gsh_state *sh);

int gsh_switch(struct gsh_state *sh, char *pathname, char *const *args);