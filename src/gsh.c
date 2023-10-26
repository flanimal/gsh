#include <unistd.h>
#include <limits.h>
#include <envz.h>
#include <sys/wait.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "gsh.h"
#include "input.h"
#include "parse.h"
#include "history.h"
#include "builtin.h"
#include "process.h"

#define GSH_PROMPT "@ "
#define GSH_WORKDIR_PROMPT(cwd) "\033[46m" cwd "\033[49m" GSH_PROMPT
#define GSH_SECOND_PROMPT "> "

#ifndef NDEBUG
bool g_gsh_initialized = false;
#endif

extern char **environ;

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

void gsh_put_prompt(const struct gsh_state *sh)
{
	if (sh->shopts & GSH_OPT_PROMPT_STATUS)
		printf("<%d> ", (WIFEXITED(sh->params.last_status)) ?
					WEXITSTATUS(sh->params.last_status) :
					255);

	if (!(sh->shopts & GSH_OPT_PROMPT_WORKDIR)) {
		printf(GSH_PROMPT);
		return;
	}

	const bool in_home = strncmp(sh->cwd,
				     gsh_getenv(&sh->params, "HOME"),
				     sh->params.home_len) == 0;

	printf((in_home) ? GSH_WORKDIR_PROMPT("~%s") : GSH_WORKDIR_PROMPT("%s"),
	       &sh->cwd[(in_home) ? sh->params.home_len : 0]);
}

void gsh_bad_cmd(const char *msg, int err)
{
	printf("not a command%s %s %s%s%s\n", (msg ? ":" : ""),
	       (msg ? msg : ""), (err ? "(" : ""), (err ? strerror(err) : ""),
	       (err ? ")" : ""));
}

const char *gsh_getenv(const struct gsh_params *params, const char *name)
{
	assert(params->env_len > 0);

	const char *value = envz_get(*environ, params->env_len, name);
	return (value ? value : "");
}

void gsh_getcwd(struct gsh_state *sh)
{
	if (getcwd(sh->cwd, (size_t)sh->max_path))
		return;

	/* Current working path longer than max_path chars. */
	free(sh->cwd);

	// We will use the buffer allocated by `getcwd`
	// to store the working directory from now on.
	sh->cwd = getcwd(NULL, 0);
	sh->max_path = pathconf(sh->cwd, _PC_PATH_MAX);
}

/*	The maximum length of an input line on the terminal
 *	that will currently be accepted, not including the newline
 *	or null byte.
 */
size_t gsh_max_input(const struct gsh_input_buf *inputbuf)
{
	return (size_t)inputbuf->max_input - inputbuf->len;
}

static void gsh_set_params(struct gsh_params *params)
{
	params->env_len = 0;
	for (char **env_it = environ; *env_it; ++env_it)
		params->env_len += strlen(*env_it);

	params->home_len = strlen(gsh_getenv(params, "HOME"));

	params->last_status = 0;
}

static void gsh_set_shopts(struct hsearch_data **shopt_tbl)
{
	struct gsh_shopt {
		char *cmd;
		enum gsh_shopt_flags flag;
	};

	static struct gsh_shopt shopts[] = {
		{ "prompt_workdir", GSH_OPT_PROMPT_WORKDIR },
		{ "prompt_status", GSH_OPT_PROMPT_STATUS },
		{ "echo", GSH_OPT_ECHO },
	};

	create_hashtable(shopts, .cmd, .flag, *shopt_tbl);
}

static struct gsh_input_buf *gsh_new_inputbuf()
{
	struct gsh_input_buf *input = malloc(sizeof(*input));
	// Get maximum length of terminal input line.
	input->max_input = fpathconf(STDIN_FILENO, _PC_MAX_INPUT);

	// Max input line length + newline + null byte.
	input->line = malloc((size_t)input->max_input + 2);

	return input;
}

void gsh_init(struct gsh_state *sh, const struct gsh_parse_bufs *parsebufs)
{
	gsh_set_builtins(&sh->builtin_tbl);
	gsh_set_shopts(&sh->shopt_tbl);
	gsh_set_params(&sh->params);

	// Get working dir and its max path length.
	sh->cwd = malloc((size_t)(sh->max_path = _POSIX_PATH_MAX));
	gsh_getcwd(sh);

	sh->inputbuf = gsh_new_inputbuf();
	sh->hist = gsh_new_hist();

	gsh_set_parse_state(&sh->parse_state, parsebufs);

	sh->shopts = GSH_OPT_DEFAULTS;

#ifndef NDEBUG
	g_gsh_initialized = true;
#endif
}

/* Fork and exec a program. */
static int gsh_exec(char *pathname, char *const *args)
{
	pid_t cmd_pid = fork();

	if (cmd_pid != 0) {
		int status;
		waitpid(cmd_pid, &status, 0);

		return status;
	}

	execvp(pathname, args);

	// Named program couldn't be executed.
	gsh_bad_cmd(pathname, errno);
	exit(GSH_EXIT_NOTFOUND);
}

static void gsh_switch(struct gsh_state *sh, char *pathname, char *const *args)
{
	// TODO: Should check for atl one argument be done in here?
	if (strcmp(args[0], "exit") == 0)
		exit(EXIT_SUCCESS);

	ENTRY *builtin;
	if (hsearch_r((ENTRY){ .key = args[0] }, FIND, &builtin,
		      sh->builtin_tbl))
		sh->params.last_status = GSH_BUILTIN_FUNC(builtin)(sh, args);
	else
		sh->params.last_status = gsh_exec(pathname, args);
}

static void gsh_set_opt(struct gsh_state *sh, char *name, bool value)
{
	ENTRY *result;
	if (!hsearch_r((ENTRY){ .key = name }, FIND, &result, sh->shopt_tbl))
		return;

	const enum gsh_shopt_flags flag = *(enum gsh_shopt_flags *)result->data;

	if (value)
		sh->shopts |= flag;
	else
		sh->shopts &= ~flag;
}

/*
	You don't want to have to specify explicitly what to do if
	a token or part of token isn't found. It's verbose and clumsy.

	*** For our purposes, a "word" is a contiguous sequence of characters
		NOT containing whitespace.
*/
static void gsh_process_opt(struct gsh_state *sh, char *shopt_ch)
{
	if (!isalnum(shopt_ch[1])) {
		// There wasn't a name following the '@' character,
		// so remove the '@' and continue.
		*shopt_ch = ' ';
		return;
	}

	char *valstr = strpbrk(shopt_ch + 1, WHITESPACE);
	char *after = valstr;

	if (valstr && isalpha(valstr[1])) {
		*valstr++ = '\0';

		const int val = (strncmp(valstr, "on", 2) == 0)  ? true :
				(strncmp(valstr, "off", 3) == 0) ? false :
									-1;
		if (val != -1) {
			after = strpbrk(valstr, WHITESPACE);
			gsh_set_opt(sh, shopt_ch + 1, val);
		}
	}

	if (!after) {
		*shopt_ch = '\0';
		return;
	}

	while (shopt_ch != after + 1)
		*shopt_ch++ = ' ';
}

void gsh_run_cmd(struct gsh_state *sh)
{
	assert(g_gsh_initialized);

	if (strcspn(sh->inputbuf->line, WHITESPACE) == 0) {
		sh->inputbuf->len = 0;
		return;
	}

	gsh_add_hist(sh->hist, sh->inputbuf->len, sh->inputbuf->line);

	// Change shell options first.
	//
	// NOTE: Because this occurs before any other parsing or tokenizing,
	// it means that "@" characters will be interpreted as shell options
	// even inside quotes.
	// 
	// The solution might be to only count words _beginning with_ the '@'
	// character as option assignments.
	// So, 
	//	If '@' occurs at beginning of line, OR
	//	If '@' occurs immediately after whitespace (beginning of new word)
	// Except that won't necessarily work -- what if the '@' follows whitespace,
	// but within quotes? It will still be processed.
	//
	// The _real_ solution might be that we have to split the line into words
	// separately from parsing them. Split first, then process options, then parse.
	//
	for (char *shopt = sh->inputbuf->line; (shopt = strchr(shopt, '@'));)
		gsh_process_opt(sh, shopt);

	char *const *argv = gsh_parse_cmd(sh->parse_state, &sh->params,
					  &sh->inputbuf->line);
	if (argv)
		gsh_switch(sh, sh->inputbuf->line, argv);
	
	sh->inputbuf->len = 0;
}
