#pragma once

enum gnuish_built_in
{
	EXIT,
	CHDIR,
	KILL,
	PWD,
	ECHO,
	HIST,
	HELP,
	RECALL,
};

struct gnuish_past_cmd
{
	char* line;
	struct gnuish_past_cmd* next;
};

struct gnuish_state
{
	struct gnuish_past_cmd* cmd_history;

	char* last_line;

	char** env;

	long max_input;
};

ssize_t gnuish_read_line(struct gnuish_state* sh_state, char* out_line);

void gnuish_chdir(struct gnuish_state* sh_state, const char* pathname);

void gnuish_recall(struct gnuish_state* sh_state, int n);

void gnuish_run_cmd(struct gnuish_state* sh_state, const char* line);

void gnuish_exec(struct gnuish_state* sh_state, char** args);

void gnuish_exit(struct gnuish_state* sh_state);