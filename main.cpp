#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>

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

namespace gnuish
{
	void exit_shell()
	{
		exit(0);
	}

	void echo()
	{

	}

	void kill()
	{

	}

	void help()
	{

	}
}

int main(int argc, char* argv[], char* envp[])
{
	// Upon typing the name of this program in Bash, a child process will be forked
	// and the process image replaced with this program. So we need not do that ourselves.
	
	// ***
	
	/* Main loop. */
	char line[255];
	char** args = new char* [255];

	for (std::size_t nbytes; (nbytes = read(STDIN_FILENO, &line, 255)) > 0; )
	{
		// We must handle user input.
		// We look at [?] for keypresses.
		// Upon receipt of a carriage return (CR) character, we fork a child process, 
		
		write(STDOUT_FILENO, &line, nbytes);
		
		// and in that process we take the preceding line of input and pass it to `execve(2)` or `exec(3)`. 
		
		char* pathname = strtok(line, " \n"); // Must remember to include newline as delimiter!

		args[0] = pathname;

		int argNum = 1;
		while ((args[argNum++] = strtok(nullptr, " ")))
			;

		args[argNum] = nullptr;

		pid_t cmdChild = fork();

		if (cmdChild)
			execve(pathname, args, envp);
		else
			waitpid(cmdChild, nullptr, 0);
		
		// !!! I seem to have accidentally created a fork bomb or something.

		//	The line of input will then be pushed onto the history stack.
		//	If this will result in more than 10 lines on the stack, the oldest one will be popped.
		//	*** If we use a stack, how will we execute the "n"th last command without popping?

	}

	printf("Process ending\n");
	return 0;
}