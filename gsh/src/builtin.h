#pragma once

#include "process.h"

typedef int (*gsh_builtin_callback)(struct gsh_state *, char *const *);

struct gsh_cb_wrapper {
	gsh_builtin_callback cb;
};

#define GSH_BUILTIN_ENTRY(callback) \
	((struct gsh_cb_wrapper *)callback->data)->cb

void gsh_set_builtins(struct hsearch_data **builtin_tbl);