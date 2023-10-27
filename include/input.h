#pragma once

#include <stddef.h>

struct gsh_input_buf {
	// Constants relating to terminal input.
	long max_input;

	// Buffer for terminal input.
	size_t len;
	char line[];
};

size_t gsh_max_input(const struct gsh_input_buf *inputbuf);