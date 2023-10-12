#pragma once

#include "process.h"

typedef int (*gsh_builtin_func)(struct gsh_state *, char *const *);

struct gsh_builtin_wrapper {
	gsh_builtin_func func;
};

#define GSH_BUILTIN_FUNC(builtin) \
	((struct gsh_builtin_wrapper *)builtin->data)->func

void gsh_set_builtins(struct hsearch_data **builtin_tbl);