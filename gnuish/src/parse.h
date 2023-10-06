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

/*      Substitute a parameter reference with its value.
 */
void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed);

/*      Expand the last token.
 */
void gsh_expand_tok(struct gsh_params *params, struct gsh_parsed *parsed);

// TODO: strtok_r

/*      Returns true while there are still more tokens to collect,
 *	similar to strtok.
 *
 *      Increments token_n and token_it by 1 for each completed token.
 *
 *      ***
 *
 *      By default, a backslash \ is the _line continuation character_.
 *
 *      When it is the last character in an input line, it invokes a
 *      secondary prompt for more input, which will be concatenated to the first
 *      line, and the backslash \ will be excluded.
 *
 *      Or, in other words, it concatenates what appears on both sides of it,
 *      skipping null bytes and newlines, but stopping at spaces.
 *
 *      Or, in other *other* words, it means to append to the preceding token,
 *      stopping at spaces.
 */
bool gsh_next_tok(struct gsh_parsed *parsed, char *const line);

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