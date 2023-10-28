#pragma once

#include <stddef.h>

struct gsh_input_buf {
	/*	The maximum length of an input line to get from the terminal, 
	 *	including the newline, but NOT the NUL byte.
	 */
	long max_input;

	/* Buffer for terminal input. */
	size_t len;
	char line[];
};

struct gsh_input_buf *gsh_new_inputbuf();

size_t gsh_max_input(const struct gsh_input_buf *inputbuf);

/*	Get a zero-terminated line of input from the terminal,
 *	excluding the newline.
 */
bool gsh_read_line(struct gsh_input_buf *inputbuf);