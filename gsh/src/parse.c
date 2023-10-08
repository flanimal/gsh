#define _GNU_SOURCE

#include <envz.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "gsh.h"
#include "parse.h"

void gsh_init_parsed(struct gsh_parsed *parsed)
{
	parsed->token_it = parsed->tokens =
		calloc(GSH_MAX_ARGS, sizeof(char *));
	parsed->token_n = 0;

	// MAX_ARGS plus sentinel.
	parsed->alloc = malloc(sizeof(char *) * (GSH_MAX_ARGS + 1));
	*parsed->alloc++ = NULL; // Set empty sentinel.

	parsed->tok_state = NULL;
	parsed->need_more = false;
}

static void gsh_expand_alloc(struct gsh_parsed *parsed, size_t fmt_len,
			     size_t expand_len)
{
	if (fmt_len >= expand_len)
		return;

	size_t new_len = expand_len - fmt_len;

	if (*parsed->alloc) {
		new_len += strlen(*parsed->alloc);
		*parsed->alloc = realloc(*parsed->alloc, new_len + 1);
	} else {
		*parsed->alloc = strcpy(malloc(new_len + 1), *parsed->token_it);
		*parsed->token_it = *parsed->alloc;
	}
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed,
			  char *const fmt_begin)
{
	const size_t fmt_len = strcspn(fmt_begin + 1, "$") + 1;

	char *tmp = NULL;
	if (fmt_begin[fmt_len] != '\0')
		tmp = strdup(fmt_begin + fmt_len);

	switch (fmt_begin[1]) {
	case '?': {
		gsh_expand_alloc(parsed, fmt_len,
				 (size_t)snprintf(NULL, 0, "%d",
						  params->last_status));

		sprintf(fmt_begin, "%d%s", params->last_status,
			(tmp ? tmp : ""));
		break;
	}
	default: {
		// If whole token is a parameter reference, substitute
		// with pointer to env variable value.
		if (strcmp(*parsed->token_it, fmt_begin) == 0) {
			char *const value = envz_get(*environ, params->env_len,
						     fmt_begin + 1);

			*parsed->token_it = value ? value : "";
			break;
		}

		char *const var_name = strndup(fmt_begin + 1, fmt_len - 1);

		char *value = envz_get(*environ, params->env_len, var_name);
		free(var_name);

		if (!value)
			value = "";

		gsh_expand_alloc(parsed, fmt_len,
				 (size_t)snprintf(NULL, 0, "%s", value));

		sprintf(fmt_begin, "%s%s", value, (tmp ? tmp : ""));
		break;
	}
	}

	free(tmp);
}

/*      Expand the last token.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_tok(struct gsh_params *params, struct gsh_parsed *parsed)
{
	// TODO: globbing, piping

	char *const fmt_begin = strpbrk(*parsed->token_it, "$~");

	if (!fmt_begin)
		return false;

	switch (*fmt_begin) {
	case '$':
		gsh_fmt_param(params, parsed, fmt_begin);
		break;
	case '~':
		if (strcmp(*parsed->token_it, "~") == 0) {
			// Just subsitute the token with a reference to HOME.
			*parsed->token_it = params->homevar;
			break;
		}

		if (*parsed->alloc) {
			char *tmp;
			asprintf(&tmp, "%s%s%s", fmt_begin - 1, params->homevar,
				 fmt_begin + 1);

			free(*parsed->alloc);
			*parsed->alloc = tmp;

			break;
		}

		asprintf(parsed->alloc, "%s%s%s", fmt_begin - 1,
			 params->homevar, fmt_begin + 1);

		*parsed->token_it = *parsed->alloc;
		break;
	}

	return true;
}

/*      Returns true while there are still more tokens to collect,
 *	similar to strtok.
 *
 *      Increments token_n and token_it by 1 for each completed token.
 *
 *      ***
 *
 *      By default, a backslash \ is the _line continuation character_.
 *
 *      Or, in other words, it concatenates what appears on both sides of it,
 *      skipping null bytes and newlines, but stopping at spaces.
 *
 *      Or, in other *other* words, it means to append to the preceding token,
 *      stopping at spaces.
 */
static bool gsh_next_tok(struct gsh_params *params, struct gsh_parsed *parsed,
			 char **const line)
{
	char *const next_tok = strtok_r(
		(parsed->need_more || parsed->token_n == 0 ? *line : NULL), " ",
		&parsed->tok_state);

	if (!next_tok)
		// Reached the null byte, meaning there weren't any
		// continuations. No more tokens available or needed.
		return false;

	*line = next_tok;
	*parsed->token_it = next_tok;
	parsed->need_more = false;

	char *line_cont = strchr(next_tok, '\\');
	if (line_cont) {
		if (!line_cont[1]) {
			parsed->need_more = true;
			return true;
		}
		// Remove the backslash.
		for (; *line_cont; ++line_cont)
			line_cont[0] = line_cont[1];
	}

	while (gsh_expand_tok(params, parsed))
		;

	if (*parsed->alloc)
		++parsed->alloc;

	++parsed->token_it;
	++parsed->token_n;

	return true;
}

bool gsh_parse_filename(struct gsh_params *params, struct gsh_parsed *parsed,
			char *line)
{
	// Immediately return if we already got the filename.
	if (parsed->token_n > 0)
		return false;

	if (gsh_next_tok(params, parsed, &line) && parsed->need_more)
		return true;

	char *last_slash = strrchr(line, '/');
	if (last_slash)
		*parsed->token_it = last_slash + 1;

	parsed->has_pathname = !!last_slash;
	return false;
}

bool gsh_parse_args(struct gsh_params *params, struct gsh_parsed *parsed,
		    char **line)
{
	while (parsed->token_n <= GSH_MAX_ARGS &&
	       gsh_next_tok(params, parsed, line)) {
		if (parsed->need_more)
			return true;
	}

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

	parsed->tok_state = NULL;
	parsed->need_more = false;
}
