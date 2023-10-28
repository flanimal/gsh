#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gsh.h"
#include "history.h"
#include "builtin.h"
#include "parse.h"

int gsh_recall(struct gsh_state *sh, int argc, char *const *args);
int gsh_list_hist(struct gsh_state *sh, int argc, char *const *args);

// TODO: [ ] pwd builtin? Use for prompt as well?
//
// For example:
//

//
// When we do "help echo", for example, it should look something like this:
//
//	<builtin name> : <builtin syntax>
//	<description string>
//
//	[argument description] ...
//
static int gsh_echo(struct gsh_state *sh, int argc, char *const *argv)
{
	for (++argv; *argv; ++argv) {
		if (fputs(*argv, stdout) == EOF)
			return -1;

		if (argv[1])
			putchar(' ');
	}

	putchar('\n');

	return 0;
}

static int gsh_type(struct gsh_state *sh, int argc, char *const *argv)
{
	// FIXME: Don't depend on order of arguments and switches.
	if (argc < 3 || strcmp(argv[2], "-f") != 0) {
		ENTRY *builtin;
		if (hsearch_r((ENTRY){ .key = argv[1] }, FIND, &builtin,
			      sh->builtin_tbl)) {
			printf("%s: builtin\n", argv[1]);
			return 0;
		}
	}

	struct stat st;

	if (strchr(argv[1], '/') && stat(argv[1], &st) == 0 &&
	    (st.st_mode & 0111)) {
		puts(argv[1]);
		return 0;
	}

	char *pathname = malloc(sh->max_path);

	for (const char *path_it = gsh_getenv(&sh->params, "PATH"); path_it;
	     ++path_it) {
		// TODO: Isn't it odd how there's seemingly no way
		// to copy upto a certain character without iterating
		// the string twice?
		
		// FIXME: We need to subtract from the size passed to snprintf!
		snprintf(memccpy(pathname, path_it, ':', sh->max_path),
			 sh->max_path, "/%s", argv[1]);

		if (stat(pathname, &st) == 0 && (st.st_mode & 0111)) {
			printf("%s: %s\n", argv[1], pathname);
	
			free(pathname);
			return 0;
		}

		path_it = strchr(path_it + 1, ':');
		if (!path_it)
			break;
	}

	printf("%s: not found\n", argv[1]);
	
	free(pathname);
	return -1;
}

static int gsh_chdir(struct gsh_state *sh, int argc, char *const *argv)
{
	if (argc == 1) {
		chdir(gsh_getenv(&sh->params, "HOME"));
	} else if (chdir(argv[1]) == -1) {
		printf("%s: %s\n", argv[1], strerror(errno));
		return -1;
	}

	gsh_getcwd(sh);

	return 0;
}

// while COMMANDS; do COMMANDS; done
// 
// TODO: After encountering a `while` followed by a semicolon, modify
// the parse state to expect a "do", followed by commands and a semicolon,
// and then a "done".

// This is an example of a "syntactic" builtin.
static int gsh_while(struct gsh_state* sh, int argc, char* const* argv)
{
	gsh_parse_cmd(sh->cmd, sh->parse_state, &sh->params);
}

static int gsh_puthelp(struct gsh_state *sh, int argc, char *const *argv);

static struct gsh_builtin builtins[] = {
	{ "echo", "Write arguments to standard output.", gsh_echo },
	{ "r", "Execute the Nth last line.", gsh_recall },
	{ "cd", "Change the shell working directory.", gsh_chdir },
	{ "hist", "Display or clear line history.", gsh_list_hist },
	{ "help", "Display this help page.", gsh_puthelp },
	{ "type", "Display command type and location.", gsh_type },
	{ "while", "Run command while a condition is true.", gsh_while },
	{ "exit", "Exit the shell.", NULL },
};

static int gsh_puthelp(struct gsh_state *sh, int argc, char *const *argv)
{
	char dots[15];
	memset(dots, '.', sizeof(dots));

	for (size_t i = 0; i < sizeof(builtins) / sizeof(*builtins); ++i) {
		const size_t cmd_len = (size_t)printf("%s ", builtins[i].cmd);

		if (cmd_len > sizeof(dots)) {
			puts(builtins[i].helpstr);
			continue;
		}

		dots[sizeof(dots) - cmd_len] = '\0';

		printf("%s %s\n", dots, builtins[i].helpstr);
		dots[sizeof(dots) - cmd_len] = '.';
	}

	return 0;
}

void gsh_set_builtins(struct hsearch_data **builtin_tbl)
{
	create_hashtable(builtins, .cmd, , *builtin_tbl);
}
