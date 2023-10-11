#pragma once

int gsh_switch(struct gsh_state *sh, char *pathname, char **args);

struct gsh_parsed *gsh_init_parsed();

int gsh_parse_and_run(struct gsh_state *sh);