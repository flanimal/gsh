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
	/* List of tokens to be returned from parsing. */
	char **tokens;

	/* Iterator pointing to the token currently being parsed. */
	char **token_it;

	/* Stack of buffers for tokens that contain substitutions. */
	char **fmt_bufs;

	char *tok_state;
};

struct gsh_fmt_span {
	/* Beginning of format span within the current token. */
	char *begin;

	/* Length of the span, up to either the NUL byte or the next special
	 * char. */
	size_t len;

	/* A copy of the token text following the span, if any. */
	char *after;

	const char *fmt_str;
};

static bool gsh_parse_linebrk(char *line)
{
	char *linebreak = strchr(line, '\\');
	if (linebreak) {
		if (linebreak[1] == '\0') {
			*linebreak = '\0';
			return true;
		}
		// Remove the backslash.
		for (; *linebreak; ++linebreak)
			linebreak[0] = linebreak[1];
	}

	return false;
}

bool gsh_read_line(struct gsh_state *sh, size_t *input_len)
{
	assert(g_gsh_initialized);

	if (!fgets(sh->line + *input_len, (int)(gsh_max_input(sh) - *input_len) + 1, stdin)) {
		if (ferror(stdin))
			perror("gsh exited");

		exit(feof(stdin) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	char *newline = strchr(sh->line + *input_len, '\n');
	*newline = '\0';

	bool need_more = gsh_parse_linebrk(sh->line + *input_len);
	*input_len = (size_t)(newline - (sh->line + *input_len));

	if (need_more) {
		fputs(GSH_SECOND_PROMPT, stdout);
		--(*input_len); // Exclude backslash.	
	}

	return need_more;
}

struct gsh_parsed *gsh_init_parsed()
{
	struct gsh_parsed *parsed = malloc(sizeof(*parsed));

	parsed->token_it = parsed->tokens =
	    calloc(GSH_MAX_ARGS, sizeof(char *));

	// MAX_ARGS plus sentinel.
	parsed->fmt_bufs = calloc(GSH_MAX_ARGS + 1, sizeof(char *));

	return parsed;
}

/*	Allocate and return a new format buffer.
 */
static char *gsh_alloc_fmtbuf(struct gsh_parsed *parsed, size_t new_len)
{
	if (parsed->fmt_bufs[1]) {
		parsed->fmt_bufs[1] = realloc(parsed->fmt_bufs[1], new_len + 1);
	} else {
		parsed->fmt_bufs[1] = malloc(new_len + 1);

		strcpy(parsed->fmt_bufs[1], *parsed->token_it);
	}

	return parsed->fmt_bufs[1];
}

/*	Allocate an expansion buffer and format it using reference to args.
 */
static void gsh_expand_alloc(struct gsh_parsed *parsed,
			     struct gsh_fmt_span *span, size_t print_len,
			     va_list fmt_args)
{
	*span->begin = '\0';

	const size_t before_len = strlen(*parsed->token_it);

	char *fmtbuf = gsh_alloc_fmtbuf(parsed, before_len + print_len +
					strlen(span->after));

	*parsed->token_it = fmtbuf;
	fmtbuf += before_len;

	vsprintf(fmtbuf, span->fmt_str, fmt_args);
	strcpy(fmtbuf + print_len, span->after);
}

/*	Format a span with the given args, allocating a buffer if necessary.
 */
static void gsh_expand_span(struct gsh_parsed *parsed,
			    struct gsh_fmt_span *span, ...)
{
	va_list fmt_args, tmp_args;

	va_start(fmt_args, span);
	va_copy(tmp_args, fmt_args);

	const int print_len = vsnprintf(NULL, 0, span->fmt_str, tmp_args);

	assert(print_len >= 0);

	span->after =
		(span->begin[span->len] ? strdup(span->begin + span->len) : "");

	if (span->len >= (size_t)print_len) {
		// Don't need to allocate.
		vsprintf(span->begin, span->fmt_str, fmt_args);
		strcpy(span->begin + print_len, span->after);
	} else {
		gsh_expand_alloc(parsed, span, (size_t)print_len, fmt_args);
	}

	va_end(tmp_args);
	va_end(fmt_args);

	if (span->after[0] != '\0')
		free(span->after);
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

	gsh_expand_span(parsed, span, gsh_getenv(params, var_name));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed,
			  char *const fmt_begin)
{
	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1, gsh_special_chars) + 1,
	};

	switch ((enum gsh_special_param)span.begin[1]) {
	case GSH_STATUS_PARAM:
		span.fmt_str = "%d";

		gsh_expand_span(parsed, &span, params->last_status);
		break;
	default:
		span.fmt_str = "%s";

		gsh_fmt_var(params, parsed, &span);
		break;
	}
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

	if (strcmp(*parsed->token_it, (const char[]) { GSH_HOME_CH, '\0' }) ==
	    0) {
		*parsed->token_it = homevar;
		return;
	}

	struct gsh_fmt_span span = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s", 
	};

	gsh_expand_span(parsed, &span, homevar);
}

/*      Expand the last token.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_tok(struct gsh_params *params, struct gsh_parsed *parsed)
{
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
	case GSH_SHOPT_CH:
		//gsh_set_opt();
		return true;
	}
	
	__builtin_unreachable();
}

/*      Collect and insert a fully-expanded token into the list.
 *
 *	Returns true while there are still more tokens to collect,
 *	similar to strtok.
 */
static bool gsh_next_tok(struct gsh_params *params, struct gsh_parsed *parsed,
			 char *line)
{
	char *next_tok = strtok_r(line, " ", &parsed->tok_state);
	if (!next_tok)
		return false;

	*parsed->token_it = next_tok;

	while (gsh_expand_tok(params, parsed)) ;

	if (parsed->fmt_bufs[1])
		++parsed->fmt_bufs;

	++parsed->token_it;
	return true;
}

/*      Parse the first token in the input line, and place
 *      the filename in the argument array.
 */
static bool gsh_parse_filename(struct gsh_params *params,
			       struct gsh_parsed *parsed, char *line)
{
	if (gsh_next_tok(params, parsed, line))
		return true;

	char *last_slash = strrchr(line, '/');
	if (last_slash)
		parsed->token_it[-1] = last_slash + 1;

	return false;
}

/*	Parse tokens and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 */
static void gsh_parse_cmd_args(struct gsh_params *params,
			       struct gsh_parsed *parsed)
{
	while ((parsed->token_it - parsed->tokens) <= GSH_MAX_ARGS &&
	       gsh_next_tok(params, parsed, NULL)) ;
}

void gsh_free_parsed(struct gsh_parsed *parsed)
{
	// Delete substitution buffers.
	while (*parsed->fmt_bufs) {
		free(*parsed->fmt_bufs);
		*parsed->fmt_bufs-- = NULL;
	}

	// Reset token list.
	while (parsed->token_it > parsed->tokens)
		*(--parsed->token_it) = NULL;
}

void gsh_parse_and_run(struct gsh_state *sh)
{
	gsh_parse_filename(&sh->params, sh->parsed, sh->line);
	gsh_parse_cmd_args(&sh->params, sh->parsed);

	sh->params.last_status = gsh_switch(sh, sh->line, sh->parsed->tokens);

	gsh_free_parsed(sh->parsed);
}
