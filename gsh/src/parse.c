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

/* The maximum number of arguments that can be passed on the command line. */
#define GSH_MAX_ARGS 64

#define GSH_SECOND_PROMPT "> "

#ifndef NDEBUG
extern bool g_gsh_initialized;
#endif

struct gsh_parsed {
	bool has_pathname;
	bool need_more;

	/* List of tokens to be returned from parsing. */
	char **tokens;

	/* Pointer to the token currently being constructed. */
	char **token_it;

	/* Buffers for any substitutions performed on tokens. */
	char **alloc;

	char *tok_state;
};

struct p_fmt_info {
	char *begin;
	size_t len;

	const char *fmt_str;
};

void gsh_read_line(struct gsh_state *sh)
{
	assert(g_gsh_initialized);

	// Check if called to get more input.
	if (sh->parsed->need_more) {
		--sh->input_len;
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

	sh->line[sh->input_len + len - 1] = '\0';	// Remove newline.
	sh->input_len = len - 1;
}

struct gsh_parsed *gsh_init_parsed()
{
	struct gsh_parsed *parsed = malloc(sizeof(*parsed));

	parsed->token_it = parsed->tokens =
	    calloc(GSH_MAX_ARGS, sizeof(char *));
	
	// MAX_ARGS plus sentinel.
	parsed->alloc = malloc(sizeof(char *) * (GSH_MAX_ARGS + 1));
	*parsed->alloc++ = NULL;	// Set empty sentinel.

	parsed->tok_state = NULL;
	parsed->need_more = false;

	return parsed;
}

// TODO: (?) What happens to fmt_begin after this gets called?
// After we set *fmt_begin to a null byte?
// ANSWER: After this is called, we go back up to the while loop in next_tok(),
// and expand_tok() updates fmt_begin according to token_it.
/*	Allocate or reallocate a buffer for token expansion.
*/
static void gsh_alloc_fmt(struct gsh_parsed *parsed, struct p_fmt_info *fmt, ...)
{
	va_list fmt_args, tmp_args;

	va_start(fmt_args, fmt);
	va_copy(tmp_args, fmt_args);

	if (fmt->len >= (size_t)vsnprintf(NULL, 0, fmt->fmt_str, tmp_args)) {
		// Don't need to allocate.
		vsprintf(fmt->begin, fmt->fmt_str, fmt_args);
		goto out_end;
	}
	// Copy everything before fmt_begin.
	*fmt->begin = '\0';

	char *tmp;
	asprintf(&tmp, "%s%s", *parsed->token_it, fmt->fmt_str);

	free(*parsed->alloc);
	vasprintf(parsed->alloc, tmp, fmt_args);

	free(tmp);

	*parsed->token_it = *parsed->alloc;

out_end:
	va_end(tmp_args);
	va_end(fmt_args);
}

// TODO: Are fmt_begin and parsed->token_it always synonymous/aliased?
// NO!
// 
// TODO: Function for getting value of ANY param from gsh_params?

/*	Substitute a variable reference with its value.
*	
*	If the token consists only of the variable reference, it will be
*	assigned to point to the value of the variable.
* 
*	If the variable does not exist, the token will be assigned the empty string.
*/
static void gsh_fmt_var(struct gsh_params *params, struct gsh_parsed *parsed,
		    struct p_fmt_info *fmt, char *const fmt_after)
{
	if (strcmp(*parsed->token_it, fmt->begin) == 0) {
		*parsed->token_it = gsh_getenv(params, fmt->begin + 1);
		return;
	}

	char *const var_name = strndup(fmt->begin + 1, fmt->len - 1);

	fmt->fmt_str = "%s%s";
	gsh_alloc_fmt(parsed, fmt,
		  gsh_getenv(params, var_name), (fmt_after ? fmt_after : ""));
	free(var_name);
}

/*      Substitute a parameter reference with its value.
 */
static void gsh_fmt_param(struct gsh_params *params, struct gsh_parsed *parsed,
		      char *const fmt_begin)
{
	struct p_fmt_info fmt = {
		.begin = fmt_begin,
		.len = strcspn(fmt_begin + 1,
			       (const char[]) { GSH_PARAM_CH, '\0' }) + 1,
	};

	char *const fmt_after =
	    (fmt.begin[fmt.len] ? strdup(fmt.begin + fmt.len) : NULL);

	switch ((enum gsh_special_param)fmt.begin[1]) {
	case GSH_STATUS_PARAM:
		fmt.fmt_str = "%d%s";

		gsh_alloc_fmt(parsed,
			  &fmt,
			  params->last_status, (fmt_after ? fmt_after : ""));
		break;
	default:
		gsh_fmt_var(params, parsed, &fmt, fmt_after);
		break;
	}

	free(fmt_after);
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

	struct p_fmt_info fmt = {
		.begin = fmt_begin,
		.len = 1,
		.fmt_str = "%s%s",
	};

	gsh_alloc_fmt(parsed, &fmt, homevar, fmt_begin + 1);
}

/*      Expand the last token.
 *	Returns true while there are still expansions to be performed.
 */
static bool gsh_expand_tok(struct gsh_params *params,
			    struct gsh_parsed *parsed)
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

/*      Returns true while there are still more tokens to collect,
 *	similar to strtok.
 *
 *      Increments token_n and token_it by 1 for each completed token.
 */
static bool gsh_next_tok(struct gsh_params *params, struct gsh_parsed *parsed,
			 char **const line)
{
	char *const next_tok = strtok_r((parsed->need_more
					 || parsed->token_it == parsed->tokens ? *line : NULL), " ",
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

	while (gsh_expand_tok(params, parsed)) ;

	if (*parsed->alloc)
		++parsed->alloc;

	++parsed->token_it;
	return true;
}

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
		*parsed->token_it = last_slash + 1;

	parsed->has_pathname = !!last_slash;
	return false;
}

/*	Parse tokens and place them into the argument array, which is
 *      then terminated with a NULL pointer.
 *
 *      Returns true if more input is needed to parse an argument,
 *      false if done.
 */
static bool gsh_parse_cmd_args(struct gsh_params *params, struct gsh_parsed *parsed,
			   char **line)
{
	while ((parsed->token_it - parsed->tokens) <= GSH_MAX_ARGS &&
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
	while (parsed->token_it != parsed->tokens)
		*(--parsed->token_it) = NULL;

	parsed->tok_state = NULL;
	parsed->need_more = false;
}

int gsh_parse_and_run(struct gsh_state *sh)
{
	char *line_it = sh->line;

	while (gsh_parse_filename(&sh->params, sh->parsed, sh->line) ||
	       gsh_parse_cmd_args(&sh->params, sh->parsed, &line_it)) {
		gsh_read_line(sh);
	}

	int status = gsh_switch(sh,
				(sh->parsed->has_pathname ? sh->line : NULL),
				sh->parsed->tokens);
	gsh_free_parsed(sh->parsed);

	return status;
}
