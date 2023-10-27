#pragma once

#include <stddef.h>

struct gsh_input_buf {
	// Constants relating to terminal input.
	long max_input;

	// Buffer for terminal input.
	size_t len;
	char line[];
};

struct gsh_input_buf *gsh_new_inputbuf();

size_t gsh_max_input(const struct gsh_input_buf *inputbuf);

/*	Get a zero-terminated line of input from the terminal,
 *	excluding the newline.
 */
bool gsh_read_line(struct gsh_input_buf *inputbuf);