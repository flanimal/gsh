#pragma once

#include <stddef.h>

/* Parameters. */
struct gsh_params {
	size_t env_len;

	/* Null-terminated value of HOME. */
	size_t home_len;

	int last_status;
};

const char *gsh_getenv(const struct gsh_params *params, const char *name);