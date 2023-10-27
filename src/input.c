#include <unistd.h>
#include <limits.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "input.h"

#define GSH_SECOND_PROMPT "> "

// TODO: Should this be declared (as extern) in gsh.h?
#ifndef NDEBUG
extern bool g_gsh_initialized;
#endif

struct gsh_input_buf *gsh_new_inputbuf()
{
	// Get maximum length of terminal input line.
	const long max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	// FIXME: Does _PC_MAX_INPUT include the newline??
	// Max input line length + newline + null byte.
	struct gsh_input_buf *input = malloc(sizeof(*input) + max_input + 2);
	input->max_input = max_input;

	return input;
}

// FIXME: Clean up the encapsulation here.
/*	The maximum length of an input line on the terminal
 *	that will currently be accepted, not including the newline
 *	or null byte.
 */
size_t gsh_max_input(const struct gsh_input_buf *inputbuf)
{
	return (size_t)inputbuf->max_input - inputbuf->len;
}

static bool gsh_replace_linebrk(char *line)
{
	char *linebrk = strchr(line, '\\');
	if (!linebrk)
		return false;

	if (linebrk[1] == '\0') {
		*linebrk = '\0';
		return true;
	}
	// Remove the backslash.
	for (; *linebrk; ++linebrk)
		linebrk[0] = linebrk[1];

	return false;
}

bool gsh_read_line(struct gsh_input_buf *inputbuf)
{
	assert(g_gsh_initialized);

	char *const line_it = inputbuf->line + inputbuf->len;

	// TODO: fgets() or getline()?
	// Add 1 for the newline.
	if (!fgets(line_it, (int)gsh_max_input(inputbuf) + 1, stdin)) {
		if (ferror(stdin))
			perror("gsh exited");

		exit(feof(stdin) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	char *newline = strchr(line_it, '\n');
	*newline = '\0';

	bool need_more = gsh_replace_linebrk(line_it);
	inputbuf->len = (size_t)(newline - line_it);

	if (need_more) {
		fputs(GSH_SECOND_PROMPT, stdout);
		--inputbuf->len; // Exclude backslash.
	}

	return need_more;
}