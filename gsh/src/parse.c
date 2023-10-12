#define _GNU_SOURCE

#include <envz.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "gsh.h"
#include "parse.h"

#include "special.def"

/*
 *	The maximum number of arguments that can be passed on the command line,
 * 	including the filename.
 */
#define GSH_MAX_ARGS 64

#define GSH_SECOND_PROMPT "> "

#ifndef NDEBUG
extern bool g_gsh_initialized;
#endif

struct gsh_parsed {
	bool has_pathname; // TODO: Do we still need this?
	bool need_more;

	/* List of tokens to be returned from parsing. */
	char **tokens;

	/* Pointer to the token currently being parsed. */
	char **token_it;

	/* Stack of buffers for tokens that contain substitutions. */
	char **alloc;

	char *tok_state;
};

struct gsh_fmt_span {
	/* Beginning of format span within the current token. */
	char *begin;

	/* Length of the span, up to either the NUL byte or the next special char. */
	size_t len;

	/* A copy of the token text following the span, if any. */
	char *after;

	const char *fmt_str;
};

void gsh_read_line(struct gsh_state *sh)
{
	assert(g_gsh_initialized);

	// Check if called to get more input.
	if (sh->parsed->need_more) {
		--sh->input_len; // Overwrite backslash.
		fputs(GSH_SECOND_PROMPT, stdout);
	} else {
		sh->input_len = 0;
		gsh_put_prompt(sh);
	}

	if (!fgets(sh->line + sh->input_len, (int)(gsh_max_input(sh) + 1),
		   stdin)) {
		if (ferror(stdin))
			perror("gsh exited");

		exit((feof(stdin) ? EXIT_SUCCESS : EXIT_FAILURE));
	}

	const size_t len = strlen(sh->line + sh->input_len);

	sh->line[sh->input_len + len - 1] = '\0'; // Remove newline.
	sh->input_len = len - 1;
}

struct gsh_parsed *gsh_init_parsed()
{
	struct gsh_parsed *parsed = malloc(sizeof(*parsed));

	parsed->token_it = parsed->tokens =
		calloc(GSH_MAX_ARGS, sizeof(char *));

	// MAX_ARGS plus sentinel.
	parsed->alloc = calloc(GSH_MAX_ARGS + 1, sizeof(char *));

	parsed->tok_state = NULL;
	parsed->need_more = false;

	return parsed;
}

/*	(Re)allocate an expansion buffer and format it with args.
 */
static void gsh_alloc_fmt(struct gsh_parsed *parsed, struct gsh_fmt_span *span,
			  ...)
{
	va_list fmt_args, tmp_args;

	va_start(fmt_args, span);
	va_copy(tmp_args, fmt_args);

	const int print_len = vsnprintf(NULL, 0, span->fmt_str, tmp_args);
	assert(print_len >= 0);

	if (span->len >= (size_t)print_len) {
		// Don't need to allocate.
		vsprintf(span->begin, span->fmt_str, fmt_args);
		strcpy(span->begin + print_len, span->after);

		goto out_end;
	}

	*span->begin = '\0'; // <<< TODO: (!) IMPORTANT (may want to make more
			     // prominent)

	const size_t new_len =
		strlen(*parsed->token_it) + (size_t)print_len + strlen(span->after);

	if (parsed->alloc[1])
		parsed->alloc[1] = realloc(parsed->alloc[1], new_len + 1);
	else
		strcpy((parsed->alloc[1] = malloc(new_len + 1)),
		       *parsed->token_it);

	vsprintf(parsed->alloc[1], span->fmt_str, fmt_args);
	strcpy(parsed->alloc[1] + print_len, span->after);

	*parsed->token_it = parsed->alloc[1];

out_end:
	va_end(tmp_args);
	va_end(fmt_args);
}

/*	Substitute a variable reference with its value.
 *
 *	If the token consists only of the variable reference, it will be
 *	assigned to point to the value of the variable.
 *
 *	If the variable does not exist, the token will be assigned the empty
 *string.
 */
static void gsh_fmt_var(struct gsh_params *params, struct gsh_parsed *parsed,
			struct gsh_fmt_span *span)
{
	if (strcmp(*parsed->token_it, span->begin) == 0) {
		*parsed->token_it = gsh_getenv(params, span->begin + 1);
		return;
	}

	char *const var_name = strndup(span->begin + 1, span->len - 1);

	span->fmt_str = "%s";
	span->after = (span->after ? span->after : "");

	gsh_alloc_fmt(parsed, span, gsh_getenv(params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed,
			  char *const fmt_begin)
{
	// FIXME: Make sure not undefined to use other member in init.
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1,
			       (const char[]){ GSH_PARAM_CH, '\0' }) +
		       1,
		.after = (span.begin[span.len] ? strdup(span.begin + span.len) :
						 NULL),
	};
	// TODO: (?) Only dup if we "need" to?
	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_STATUS_PARAM:
		span.fmt_str = "%d";

		gsh_alloc_fmt(parsed, &span, params->last_status);
		break;
	default:
		gsh_fmt_var(params, parsed, &span);
		break;
	}

	free(span.after);
}

/*	Substitute the home character with the value of $HOME.
*
	If the token consists only of the home character, it will be
*	assigned to point to the value of $HOME.
*/
static void gsh_fmt_home(struct gsh_params *params, struct gsh_parsed *parsed,
			 char *const fmt_begin)
{
	char *const homevar = gsh_getenv(params, "HOME");

	if (strcmp(*parsed->token_it, (const char[]){ GSH_HOME_CH, '\0' }) ==
	    0) {
		*parsed->token_it = homevar;
		return;
	}

	struct gsh_fmt_span fmt = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s",
	};

	gsh_alloc_fmt(parsed, &fmt, homevar, fmt_begin + 1);
}

/*      Expand the last token.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_tok(struct gsh_params *params, struct gsh_parsed *parsed)
{
	// TODO: globbing, piping

	char *const fmt_begin = strpbrk(*parsed->token_it, gsh_special_chars);

	if (!fmt_begin)
		return false;

	switch ((enum gsh_special_char)fmt_begin[0]) {
	case GSH_PARAM_CH:
		gsh_fmt_param(params, parsed, fmt_begin);
		return true;
	case GSH_HOME_CH:
		gsh_fmt_home(params, parsed, fmt_begin);
		return true;
	default:
		return true;
	}
}

static bool gsh_parse_linebrk(struct gsh_parsed *parsed, char *line)
{
	char *linebreak = strchr(line, '\\');
	if (linebreak) {
		if (linebreak[1] == '\0') {
			parsed->need_more = true;
			return true;
		}
		// Remove the backslash.
		for (; *linebreak; ++linebreak)
			linebreak[0] = linebreak[1];
	}

	return false;
}

/*      Collect and insert a fully-expanded token into the list.
 *
 *	Returns true while there are still more tokens to collect,
 *	similar to strtok.
 */
static bool gsh_next_tok(struct gsh_params *params, struct gsh_parsed *parsed,
			 char **const line_it)
{
	{
		char *const str = (!parsed->tokens[1] || parsed->need_more) ?
					  *line_it :
					  NULL;

		parsed->need_more = false;

		if (!(*line_it = strtok_r(str, " ", &parsed->tok_state)))
			return false;
	}

	*parsed->token_it = *line_it;

	if (gsh_parse_linebrk(parsed, *line_it))
		return true;

	while (gsh_expand_tok(params, parsed))
		;

	if (parsed->alloc[1])
		++parsed->alloc;

	++parsed->token_it;
	return true;
}

// TODO: (!) Move filename getting outside of its own function and into
// gsh_parse_and_run?

/*      Parse the first token in the input line, and place
 *      the filename in the argument array.
 *
 *      Returns true if more input is needed to parse a filename,
 *      false if done.
 */
static bool gsh_parse_filename(struct gsh_params *params,
			       struct gsh_parsed *parsed, char *line)
{
	// Immediately return if filename has already been gotten.
	if (parsed->token_it != parsed->tokens)
		return false;

	if (gsh_next_tok(params, parsed, &line) && parsed->need_more)
		return true;

	char *last_slash = strrchr(line, '/');
	if (last_slash)
		parsed->token_it[-1] = last_slash + 1;

	parsed->has_pathname = !!last_slash;
	return false;
}

/*	Parse tokens and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 *
 *      Returns true if more input is needed to parse an argument,
 *      false if done.
 */
static bool gsh_parse_cmd_args(struct gsh_params *params,
			       struct gsh_parsed *parsed, char **const line_it)
{
	while ((parsed->token_it - parsed->tokens) <= GSH_MAX_ARGS &&
	       gsh_next_tok(params, parsed, line_it)) {
		if (parsed->need_more)
			return true;
	}

	return false;
}

void gsh_free_parsed(struct gsh_parsed *parsed)
{
	// Delete substitution buffers.
	while (*parsed->alloc) {
		free(*parsed->alloc);
		*parsed->alloc-- = NULL;
	}

	// Reset token list.
	while (parsed->token_it > parsed->tokens)
		*(--parsed->token_it) = NULL;
}

int gsh_parse_and_run(struct gsh_state *sh)
{
	char *line_it = sh->line;

	for (;; gsh_read_line(sh)) {
		if (gsh_parse_filename(&sh->params, sh->parsed, sh->line))
			continue;

		if (gsh_parse_cmd_args(&sh->params, sh->parsed, &line_it))
			continue;

		break;
	}

	int status = gsh_switch(sh,
				(sh->parsed->has_pathname ? sh->line : NULL),
				sh->parsed->tokens);
	gsh_free_parsed(sh->parsed);

	return status;
}
