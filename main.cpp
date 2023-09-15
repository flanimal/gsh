#include <iostream>
#include <unistd.h>

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
*/

int main()
{
	std::cout << "One\n";

	fork();

	std::cout << "Two\n";

	return 0;
}