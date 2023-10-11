#pragma once

int gsh_switch(struct gsh_state *sh, char *pathname, char **args);

struct gsh_parsed *gsh_init_parsed();

void gsh_free_parsed(struct gsh_parsed *parsed);

int gsh_parse_and_run(struct gsh_state *sh);

size_t gsh_max_input(const struct gsh_state *sh);

void gsh_put_prompt(const struct gsh_state *sh);