#include <search.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gsh.h"
#include "history.h"

#define GSH_MAX_HIST 10

/* Line history entry. */
struct gsh_hist_ent {
	struct gsh_hist_ent *back, *forw;

	char *line;
	size_t len;
};

void gsh_bad_cmd(const char *msg, int err);
void gsh_free_parsed(struct gsh_parsed *parsed); // FIXME:

static void new_hist_ent(struct gsh_cmd_hist *sh_hist,
					 size_t len, const char *line)
{
	struct gsh_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_hist->cmd_history);
	sh_hist->cmd_history = last_cmd;

	last_cmd->line = strcpy(malloc(len + 1), line);
	last_cmd->len = len;

        if (sh_hist->hist_n == 0)
		sh_hist->oldest_cmd = last_cmd;

	++sh_hist->hist_n;
}

static void drop_hist_ent(struct gsh_cmd_hist *sh_hist,
			  struct gsh_hist_ent *dropped_ent)
{
	sh_hist->oldest_cmd = dropped_ent->back;

	remque(dropped_ent);

	free(dropped_ent->line);
	free(dropped_ent);

	--sh_hist->hist_n;
}

void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len,
			 const char *line)
{
	// The recall command `r` itself should NOT be added to history.
	if (line[0] == 'r' && (!line[1] || isspace(line[1])))
		return;

	new_hist_ent(sh_hist, len, line);

	if (sh_hist->hist_n > GSH_MAX_HIST)
		drop_hist_ent(sh_hist, sh_hist->oldest_cmd);
}
// TODO: Clear history option
int gsh_list_hist(const struct gsh_cmd_hist *sh_hist)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;

	for (struct gsh_hist_ent *cmd_it = sh_hist->cmd_history; cmd_it; cmd_it = cmd_it->forw)
		printf("%d: %s\n", cmd_n++, cmd_it->line);

	return 0;
}

/* Re-run the n-th previous line of input. */
int gsh_recall(struct gsh_state *sh, const char *recall_arg)
{
	int n_arg = (recall_arg ? atoi(recall_arg) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gsh_bad_cmd("no matching history entry", 0);
		return -1;
	}

	struct gsh_hist_ent *cmd_it = sh->hist->cmd_history;

	while (cmd_it->forw && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);

	// Ensure that parse state from recall invocation is not
	// reused.
	gsh_free_parsed(sh->parsed);

	// Make a copy so we don't lose it if the history entry
	// gets deleted.
	strcpy(sh->line, cmd_it->line);
	sh->input_len = cmd_it->len;

	gsh_run_cmd(sh);

	return sh->params.last_status;
}