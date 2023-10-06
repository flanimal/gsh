#define _GNU_SOURCE

#include <envz.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gnuish.h"
#include "parse.h"

void gsh_init_parsed(struct gsh_parsed *parsed)
{
	parsed->token_it = parsed->tokens =
		calloc(GSH_MAX_ARGS, sizeof(char *));
	parsed->token_n = 0;

	// MAX_ARGS plus sentinel.
	parsed->alloc = malloc(sizeof(char *) * (GSH_MAX_ARGS + 1));
	*parsed->alloc++ = NULL; // Set empty sentinel.
}

void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed)
{
	switch (parsed->token_it[-1][1]) {
	case '?':
		asprintf(parsed->alloc, "%d", params->last_status);

		parsed->token_it[-1] = *parsed->alloc++;
		break;
	default: // Non-special.
		parsed->token_it[-1] = envz_get(*environ, params->env_len,
						&parsed->token_it[-1][2]);
		break;
	}
}

void gsh_expand_tok(struct gsh_params *params, struct gsh_parsed *parsed)
{
	// TODO: globbing, piping
	switch (parsed->token_it[-1][0]) {
	case '$':
		gsh_fmt_param(params, parsed);
		break;
	case '~':
		*parsed->alloc = malloc(strlen(parsed->token_it[-1]) +
					params->home_len + 1);

		strcpy(stpcpy(*parsed->alloc, params->homevar),
		       &parsed->token_it[-1][1]);

		parsed->token_it[-1] = *parsed->alloc++;
		break;
	}
}

bool gsh_next_tok(struct gsh_parsed *parsed, char *const line)
{
	char *const next_tok =
		strtok((*parsed->token_it ? *parsed->token_it : line), " ");

	if (!next_tok)
		// Reached the null byte, meaning there weren't any
		// continuations. No more tokens available or needed.
		return false;

	char *line_cont = strchr(next_tok, '\\');

	if (!line_cont) {
		// No line continuation, and more input available.
		// Get more tokens.
		*parsed->token_it++ = next_tok;
		++parsed->token_n;
	} else if (line_cont[1] == '\0') {
		// Line continuation followed by null byte.
		// Need more input to finish the current token.
		*parsed->token_it = next_tok;
	} else {
		// Line continuation followed by more input.
		for (; *line_cont; ++line_cont)
			line_cont[0] = line_cont[1];

		*parsed->token_it++ = next_tok;
		++parsed->token_n;
	}

	return true;
}

bool gsh_parse_filename(struct gsh_params *params,
			       struct gsh_parsed *parsed, char *line)
{
	// Immediately return if we already got the filename.
	if (parsed->token_n > 0)
		return false;

	if (gsh_next_tok(parsed, line) && *parsed->token_it)
		return true; // Need more input.

	// Perform any substitutions.
	gsh_expand_tok(params, parsed);

	char *last_slash = strrchr(line, '/');
	if (last_slash)
		*parsed->token_it = last_slash + 1;

	parsed->has_pathname = !!last_slash;
	return false;
}

bool gsh_parse_args(struct gsh_params *params, struct gsh_parsed *parsed)
{
	// Get arguments.
	while (parsed->token_n <= GSH_MAX_ARGS && gsh_next_tok(parsed, NULL)) {
		// If gsh_next_tok() returned true but we're still
		// on an incomplete token, then we need more input.
		if (*parsed->token_it)
			return true;

		gsh_expand_tok(params, parsed);
	}

	// We've gotten all the input we need to parse a line.
	return false;
}

void gsh_free_parsed(struct gsh_parsed *parsed)
{
	// Delete any token substitution buffers.
	while (parsed->alloc[-1])
		free(*parsed->alloc--);

	// Mark tokens as empty.
	for (; parsed->token_n > 0; --parsed->token_n)
		*(--parsed->token_it) = NULL;
}