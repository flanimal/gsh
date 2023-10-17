#pragma once

#include "process.h"

typedef int (*gsh_builtin_func)(struct gsh_state *, char *const *);

struct gsh_builtin {
	char *cmd;
	gsh_builtin_func func;
};

#define GSH_BUILTIN_FUNC(builtin) \
	((struct gsh_builtin *)builtin->data)->func

void gsh_set_builtins(struct hsearch_data **builtin_tbl);