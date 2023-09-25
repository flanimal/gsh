# gnuish - GNU island shell

In this directory you will find two source files and a header file. Run `make` to build the shell.

gnuish displays the current working directory in the shell prompt:
 
 	~ @
 	/usr/ @
 	/mnt/.../repos @
 
Commands

 		<command> [<args>...]	 Run command or program with optional arguments.
 
 		r [<n>]		Execute the nth last line.
 				The line will be placed in history--not the `r` invocation. 
				The line in question will be echoed to the screen before being executed.
 
 Built-ins:
 
 		exit		Exit the shell.
 
 		hist		Display up to 10 last lines entered, numbered.
 
 		----
 
 		echo		Write to stdout.
 
 		help		Display this help page.
 
See the Makefile, which:
	1. Compiles all source code.
 	2. Cleans up the directory with `make clean`.