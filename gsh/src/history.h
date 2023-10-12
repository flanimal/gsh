#pragma once

#include <stddef.h>

struct gsh_cmd_hist *gsh_init_hist();

void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len, const char *line);