#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>

#define MAXARGS 512
#define MAXLEN 2048
#define MAXPROCESSES 1000

// global variables
int processes[MAXPROCESSES];
int processNum = 0;
int processStatus;
int parentProcess = 1;
int foregroundOnly = 0; // if 1 then & (background) option is ignored
int runInForeground = 1; // run non-builtin command in runInForeground

// global struct variables
struct sigaction sa_sigint; 
struct sigaction sa_sigtstp;

// Function Declarations
void runCommand(char** argList, int nArgs, struct sigaction sa);
void runNonBuiltInCommand(char** argList, int nArgs, int runInForeground); 
void catchSIGINT(int signo);
void catchSIGTSTP(int signo);

void runCommand(char** argList, int nArgs, struct sigaction sa) {
	
	pid_t spawnpid;
	int exitStatus = 0;
	
	// run built in commands: &, exit, cd, and status
	// built in command: & 
	if (strncmp(argList[nArgs - 1], "&", 1) == 0) {
		if (foregroundOnly == 0)  {
			runInForeground = 0;
		}
		argList[nArgs - 1] = NULL;
		nArgs--;
	}
	
	// parse through each process and kill it
	if (strncmp(argList[0], "exit", 4) == 0) {
		if (processNum == 0) {
			exit(0);
		}
		else {
			for (int i = 0; i < processNum; i++) {
				kill(processes[i], SIGTERM);
			}
			exit(1);
		}
	}

	// cd to home if dir not specified 
	else if (strncmp(argList[0], "cd", 2) == 0) {
		char cwd[MAXLEN];
		if (nArgs == 1) {
			chdir(getenv("HOME"));
		}
		else {
			chdir(argList[1]);
		}


	}
	
	// check the last status and print the status value
	else if (strncmp(argList[0], "status", 6) == 0) {
		waitpid(getpid(), &processStatus, 0);
		if (WIFEXITED(processStatus)) {
			fprintf(stderr, "last exited with %d\n", WEXITSTATUS(processStatus));
		}

		if (WIFSIGNALED(processStatus)) {
			fprintf(stderr, "last exited with signal %d\n", WTERMSIG(processStatus));

		}

		return;
	}

	else {
		if (processNum + 1 > MAXPROCESSES) {
			fprintf(stderr, "reached max number of child processes, wait and try again later\n");
			return;
		}
		
		else 
		{
			// run non built in command
			// fork process to create new child
			spawnpid = fork();
			processes[processNum] = (intmax_t) spawnpid;
			processNum++;
			
			switch (spawnpid)
			{
				case -1:
					perror("Hull Breach!");
					fflush(stdout);
					exit(1);
					break;
				
				// handle sucessful child creation
				case 0:
					if (runInForeground) {
						sa_sigint.sa_handler=SIG_DFL;
					}
					sigaction(SIGINT, &sa_sigint, NULL);
					parentProcess = 0;
					runNonBuiltInCommand(argList, nArgs, runInForeground);
					fflush(stdout);
					break;
				
				// handle successful parent process
				default:
					parentProcess = 1;
			
					// if background process
					if (!runInForeground) {
						fflush(stdout);
						waitpid(spawnpid, &processStatus, WNOHANG);
						fprintf(stderr, "background pid is %d\n", spawnpid);
					}

					// if foreground process
					if (runInForeground) {
						// prevent shell input while command is running
						waitpid(spawnpid, &processStatus, 0);

					}
					break;
			}
		}
		
		// if a background process has completed print out its pid and exit/termination status
		while ((spawnpid = waitpid(-1, &processStatus, WNOHANG)) > 0) {
			if (foregroundOnly == 0) {
				if (WIFEXITED(processStatus)) {
					fprintf(stderr, " background pid %d is done: exit value %d\n", spawnpid, WEXITSTATUS(processStatus));
				}
				if (WIFSIGNALED(processStatus)) {
					fprintf(stderr, "background pid %d is done: termianted by signal %d\n", spawnpid, WTERMSIG(processStatus));
				}
				fflush(stdout);
			}
		}
	}
}

// runs non built in commands and handles input/output redirection
void runNonBuiltInCommand(char** argList, int nArgs, int runInForeground) {
	char inputFile[MAXLEN], targetFile[MAXLEN];
	
	// go through arg list and do input/output redirections
	for (int i = 0; i < nArgs; i++) {
		if (strncmp(argList[i], "<", 1) == 0 && (i + 1) < nArgs) {
			argList[i] = NULL;
			if (argList[i+1])
			{
				strcpy(inputFile, argList[i+1]);
				i++;
			}
		}

		else if (strncmp(argList[i], ">", 1) == 0 && (i + 1) < nArgs) {
			argList[i] = NULL;
			if (argList[i+1]) {
				strcpy(targetFile, argList[i + 1]);
				i++;
			}
		}

	}
	
	// if running in background and user doesn't redirect then redirect should be /dev/null
	if (runInForeground != 1) {
		if (strlen(inputFile) == 0 ) {
			strcpy(inputFile, "/dev/null");
		}
		if (strlen(targetFile) == 0) {
			strcpy(targetFile, "/dev/null");
		}
	}
	
	// opens input file and handles invalid input file 
	if (strlen(inputFile) > 0) {
		int inFD = open(inputFile, O_RDONLY);
		if (inFD == -1) {
			perror("source open()");
			exit(1);	
		}
		if(dup2(inFD, STDIN_FILENO) == -1) {
			fprintf(stderr, "unable to duplicate file describtor");
			exit(2);
		};
		close(inFD);	
	}
	
	// opens output file and handles invalid output file
	if (strlen(targetFile) > 0) {
		int targetFD = open(targetFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (targetFD == -1) {
			perror("target open()");
			exit(1);
		}
		if (dup2(targetFD, STDOUT_FILENO) == -1) {
			fprintf(stderr, "unable to duplicate file descriptor");
			exit(2);
		}
		close(targetFD);
	}

	// execute command
	execvp(argList[0], argList); 
	perror(argList[0]);

}

// custom handler for SIGINT (CTRL C)
// print out termination signal status
void catchSIGINT(int signo) {
	fprintf(stderr, "\nterminated by signal %d\n", signo);
	fflush(stdout);
	return;
}

// custom handler for SIGTSTP (CTRL Z)
// toggles shell from foreground only mode to foreground/background mode and vice versa
void catchSIGTSTP(int signo) {
	if (parentProcess = 1) {
		if (foregroundOnly == 0) {
			foregroundOnly = 1;
			write(1, "\nEntering runInForeground-only mode (& is now ignored)\n", 55);
			fflush(stdout);
		
		}
		else {
			foregroundOnly = 0;
			write(1, "\nExiting runInForeground-only mode\n", 30);
			fflush(stdout);
		}
	}
	return;
}

// main function prompts for user input, reads the input, and parses the input
// handles $$ expansion and calls runCommand function to process user input
int main(int argc, char *argv[]) {
	char* argList[MAXARGS];

	// Ignore ^C
	sa_sigint.sa_handler = catchSIGINT;
	sigfillset(&sa_sigint.sa_mask);
	sa_sigint.sa_flags = 0;
	sigaction(SIGINT, &sa_sigint, NULL);

	// redirect ^Z to catchSIGTSTP()
	sa_sigtstp.sa_handler = catchSIGTSTP;
	sigfillset(&sa_sigtstp.sa_mask);
	sa_sigint.sa_flags = 0;
	sigaction(SIGTSTP, &sa_sigtstp, NULL);

	
	while (1) {
		char buf[MAXLEN];
		char* token;
		int nArgs;

		// reset variables
		memset(argList, '\0', MAXARGS);
	 	runInForeground = 1;
		nArgs = 0;

		// prompt user for input and read input
		fprintf(stderr, ": ");
		fflush(stdout);
		fgets(buf, MAXLEN , stdin);
		strtok(buf, "\n");

		// get the command
		token = strtok(buf, " ");
		
		// ignore comments and empty lines
		if (strncmp("#", token, 1) == 0 || strcmp(buf, "\n") == 0) {
			continue;
		}

		else {
			// get the arguments
			while (token != NULL) {
				argList[nArgs] = token;
				for (int i = 1; i < strlen(argList[nArgs]); i++) {
				
					// expand $$ variable
					if (argList[nArgs][i] == '$' && argList[nArgs][i-1] == '$') {
						
						// convert pid to string and store in buffer variable
						int pid = getpid();
						char str[MAXLEN];
						sprintf(str, "%d", pid);

						int pidSize = strlen(str);
						char* newString;

						int j=0, cnt = 0;
						
						// create new string with appropriate length 
						for (j = 0; argList[nArgs][j] != '\0'; j++) {
							if (strstr(&argList[nArgs][j], "$$") == &argList[nArgs][j]) {
								cnt++;
								j += strlen("$$") - 1;
							}
						}

						newString = (char*)malloc(j + cnt*(pidSize - strlen("$$")) + 1);
						
						j = 0;
						
						// copy pid value to new string
						while (*argList[nArgs]) {
							if (strstr(argList[nArgs], "$$") == argList[nArgs]) {
								strcpy(&newString[j], str);
								//i += strlen("$$");
								j += pidSize; 
								argList[nArgs] += strlen("$$");
							}
							else {
								newString[j++] = *argList[nArgs]++;
							}

						}
						newString[j] = '\0';
						strcpy(argList[nArgs], newString);
						free(newString);
						token = strtok(NULL, " ");
						
					}
				}
				nArgs++;
				token = strtok(NULL, " ");
			}

			runCommand(argList, nArgs, sa_sigint);
		}
	}
	return 0;

}
