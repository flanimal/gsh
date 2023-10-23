#pragma once

#include <stddef.h>

struct gsh_input_buf {
	// Buffer and size for getting terminal input.
	char *line;
	size_t len;

	// Constants relating to terminal input.
	long max_input;
};

size_t gsh_max_input(const struct gsh_state *sh);