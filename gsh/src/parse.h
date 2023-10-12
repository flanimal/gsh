#pragma once

struct gsh_parsed *gsh_init_parsed();

void gsh_free_parsed(struct gsh_parsed *parsed);

size_t gsh_max_input(const struct gsh_state *sh);

/*	Get a NUL-terminated line of input from the terminal,
 *	excluding the newline.
 */
bool gsh_read_line(struct gsh_state *sh);

void gsh_parse_and_run(struct gsh_state *sh);

int gsh_switch(struct gsh_state *sh, char *pathname, char *const *args);