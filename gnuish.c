#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "gnuish.h"

/*
*	gnuish - GNU island shell
*	
*	gnuish displays the current working directory in the shell prompt:
* 
*		~ @ 
*		/usr/ @
*		/mnt/.../repos @
* 
*	Commands
* 
*		<command> [<args>...]	 Run command or program with optional arguments.
* 
*		r [<n>]		Execute the nth last line. 
*					The line will be placed in history, not the `r` invocation.
*					The line in question will be echoed to the screen before being executed.
*
*	Built-ins:
*
*		exit		Exit the shell.
* 
*		hist		Display up to 10 last lines entered, numbered.
*
*		----
* 
*		echo		Write to stdout.
* 
*		kill		Send a signal to a process.
*
*		help		Display this help page.
* 
*	Globbing
* 
*		TODO: globbing
* 
*	***
* 
*	See the "Readme" file within this directory.
*	
*	Also see the Makefile, which:
*		1. Compiles all source code.
*		2. Cleans up the directory.
*		3. Performs otehr tasks as required.
*/

#define GNUISH_PROMPT "@ "
#define GNUISH_MAX_ARGS 64

void gnuish_put_prompt()
{
	write(STDOUT_FILENO, GNUISH_PROMPT, sizeof(GNUISH_PROMPT));
}

void gnuish_chdir(struct gnuish_state* sh_state, const char* pathname)
{
	chdir(pathname);
	sh_state->max_input = pathconf(pathname, _PC_MAX_INPUT);
}

ssize_t gnuish_read_ln(struct gnuish_state* sh_state, char* out_line)
{
	gnuish_put_prompt();

	ssize_t nbytes = read(STDIN_FILENO, out_line, (size_t)sh_state->max_input);
	out_line[nbytes] = '\0'; // Must remember to include newline as delimiter!

	if (nbytes > 0)
		write(STDOUT_FILENO, &out_line, (size_t)nbytes); // Echo line of input.

	gnuish_add_hist(sh_state, out_line);

	return nbytes;
}

void gnuish_parse_ln(const char* line, char** out_args)
{
	char* lineTmp = malloc(strlen(line));
	strcpy(lineTmp, line);

	out_args[0] = strtok(lineTmp, " \n");

	for (int arg_n = 1; (out_args[arg_n++] = strtok(NULL, " ")) && arg_n <= GNUISH_MAX_ARGS; )
		;

	free(lineTmp);
}

void gnuish_add_hist(struct gnuish_state* sh_state, char* line)
{
	// Allocate memory for the node.
	struct gnuish_past_cmd* last_cmd = malloc(sizeof(struct gnuish_past_cmd));

	last_cmd->line = malloc(strlen(line));

	strcpy(last_cmd->line, line);

	last_cmd->next = sh_state->cmd_history;
	sh_state->cmd_history = last_cmd;
}

void gnuish_recall(struct gnuish_state* sh_state, int n)
{
	struct gnuish_past_cmd* cmd_it = sh_state->cmd_history;

	while (cmd_it->next && n-- > 0)
	{
		cmd_it = cmd_it->next;
	}

	gnuish_run_cmd(sh_state, cmd_it->line);
}

void gnuish_run_cmd(struct gnuish_state* sh_state, const char* line)
{
	char** args = malloc(sizeof(char*) * GNUISH_MAX_ARGS);

	gnuish_parse_ln(line, args);
	char* pathname = args[0];

	if (strcmp(pathname, "cd") == 0)
	{
		gnuish_chdir(sh_state, args[1]);
	}
	else if (strcmp(pathname, "r") == 0)
	{
		gnuish_recall(sh_state, atoi(args[1]));
	}
	else if (strcmp(pathname, "exit") == 0)
	{
		gnuish_exit(sh_state);
	}
	else
	{
		gnuish_exec(sh_state, args);
	}
}

void gnuish_exec(struct gnuish_state* sh_state, char** args)
{
	pid_t cmdPid = fork();

	if (cmdPid == 0)
	{
		execve(args[0], args, sh_state->env);
	}
	else
	{
		waitpid(cmdPid, NULL, 0);
	}
}

void gnuish_exit(struct gnuish_state* sh_state)
{
	exit(0);
}
