#include <search.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gnuish.h"
#include "history.h"

static void drop_hist_ent(struct gsh_cmd_hist *sh_hist,
			  struct gsh_hist_ent *dropped_ent)
{
	sh_hist->oldest_cmd = dropped_ent->back;

	remque(dropped_ent);

	free(dropped_ent->line);
	free(dropped_ent);
}

static struct gsh_hist_ent *new_hist_ent(struct gsh_cmd_hist *sh_hist,
					 size_t len, const char *line)
{
	struct gsh_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_hist->cmd_history);
	sh_hist->cmd_history = last_cmd;

	strcpy((last_cmd->line = malloc(len + 1)), line);
	last_cmd->len = len;

	return last_cmd;
}

void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len,
			 const char *line)
{
	// Recall `r` should NOT be added to history.
	if (line[0] == 'r' && (!line[1] || isspace(line[1])))
		return;

	struct gsh_hist_ent *last_cmd = new_hist_ent(sh_hist, len, line);

	if (sh_hist->hist_n == 10) {
		drop_hist_ent(sh_hist, sh_hist->oldest_cmd);
		return;
	}

	if (sh_hist->hist_n == 0)
		sh_hist->oldest_cmd = last_cmd;

	++sh_hist->hist_n;
}

int gsh_list_hist(const struct gsh_hist_ent *cmd_it)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;

	for (; cmd_it; cmd_it = cmd_it->forw)
		printf("%i: %s\n", cmd_n++, cmd_it->line);

	return 0;
}

// TODO: Possibly have a "prev_cmd" struct or something to pass to this,
// instead of the entire shell state.
/* Re-run the n-th previous line of input. */
int gsh_recall(struct gsh_state *sh, const char *recall_arg)
{
	struct gsh_hist_ent *cmd_it = sh->hist->cmd_history;

	int n_arg = (recall_arg ? atoi(recall_arg) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gsh_bad_cmd("no history", 0);
		return -1;
	}

	while (cmd_it && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);
	gsh_run_cmd(sh, cmd_it->len, cmd_it->line);
	return sh->params.last_status;
}