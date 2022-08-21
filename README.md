# Smallsh Project

A bash-like shell application written in C.

	Syntax: 
	command [arg1 arg2 ...] [< input_file] [> output_file] [&]
* Provides a prompt for running commands 
* Handles blank lines and comments, which are lines beginning with the # character
* Executes 3 built-in commands: exit, cd, and status
* Executes other commands using a function from the exec family of functions
* Supports input and output redirection
* Supports running commands in foreground and background processes
* Implements custom handler for 2 signals: SIGINT and SIGTSTP

#### How to compile and run the program:

    gcc -o smallsh smallsh.c
	./smallsh
