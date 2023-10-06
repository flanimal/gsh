#pragma once

#include <stddef.h>
#include <stdbool.h>

struct gsh_parsed {
	bool has_pathname;

	/* The tokens gotten from the current input line. */
	char **tokens;

	/* The token that is currently being parsed. */
	char **token_it;

	/* Number of complete tokens so far. */
	size_t token_n;

	/* Buffers for any substitutions performed on tokens. */
	char **alloc;
};

void gsh_init_parsed(struct gsh_parsed *parsed);

/*      Parse the first token in the input line, and place
 *      the filename in the argument array.
 *
 *      Returns true if we need more input to parse a filename,
 *      false if we're done.
 */
bool gsh_parse_filename(struct gsh_params *params, struct gsh_parsed *parsed,
			char *line);

/*	Parse tokens and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 *
 *      Returns true if we need more input to parse an argument,
 *      false if we're done.
 */
bool gsh_parse_args(struct gsh_params *params, struct gsh_parsed *parsed);

void gsh_free_parsed(struct gsh_parsed *parsed);