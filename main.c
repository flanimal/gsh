#include <unistd.h>
#include <limits.h>
#include <stdio.h>

#include "gnuish.h"

int main(int argc, char* argv[], char* envp[])
{
	char line[_POSIX_MAX_INPUT];
	//char** args = new char* [255];

	for (ssize_t nbytes; (nbytes = gnuish_read_line(cmdHistory, line)) > 0; )
	{
		gnuish_run_cmd(cmdHistory, args, line, envp);
	}

	printf("Process ending\n");
	return 0;
}