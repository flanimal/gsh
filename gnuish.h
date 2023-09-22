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

void gnuish_shell_prompt();

void gnuish_ch_workdir(struct gnuish_state* sh_state, const char* pathname);

ssize_t gnuish_read_line(struct gnuish_state* sh_state, char* out_line);

void gnuish_parse_line(const char* line, char** out_args);

void gnuish_add_history(struct gnuish_state* sh_state, char* line);

void gnuish_recall_history(struct gnuish_state* sh_state, int n);

void gnuish_run_cmd(struct gnuish_state* sh_state, const char* line);

void gnuish_execute(struct gnuish_state* sh_state, char** args);

void gnuish_exit_shell(struct gnuish_state* sh_state);
