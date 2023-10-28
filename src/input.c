#include <unistd.h>
#include <limits.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "input.h"

#define GSH_SECOND_PROMPT "> "

struct gsh_input_buf *gsh_new_inputbuf()
{
	// Get maximum length of terminal input line.
	const long max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	// Max input line length + NUL byte.
	struct gsh_input_buf *input = malloc(sizeof(*input) + max_input + 1);
	input->max_input = max_input;

	return input;
}

/*	The maximum length of an input line on the terminal
 *	that will currently be accepted, not including the NUL byte.
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
	char *const line_it = inputbuf->line + inputbuf->len;

	// TODO: fgets() or getline()?
	// Add 1 for the newline.
	if (!fgets(line_it, (int)gsh_max_input(inputbuf) + 1, stdin)) {
		if (ferror(stdin))
			perror("gsh exited");

		exit(feof(stdin) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	inputbuf->len = strlen(line_it);
	if (line_it[inputbuf->len - 1] == '\n')
		line_it[inputbuf->len - 1] = '\0';

	bool need_more = gsh_replace_linebrk(line_it);

	if (need_more) {
		fputs(GSH_SECOND_PROMPT, stdout);
		--inputbuf->len; // Exclude backslash.
	}

	return need_more;
}