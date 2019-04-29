#include"libshell.h"
#include<signal.h>
#include<setjmp.h>

static void jumphand(int);
static sigjmp_buf jump_to_prompt;
static volatile sig_atomic_t jump_ok;


void main(void) {
	char inbuf[MAX_CANON];
	pid_t child_pid,
	      wait_pid;
	char *backp;
	int inbackground;
	struct sigaction jumphdl;
	struct sigaction defaulthndl;
	sigset_t blockmask;

	// set mask
	sigemptyset(&blockmask);
	sigaddset(&blockmask, SIGINT);
	sigaddset(&blockmask, SIGQUIT);

	// set handlers
	jumphdl.sa_handler = jumphand;
	jumphdl.sa_mask = blockmask;
	jumphdl.sa_flags = 0;

	defaulthndl.sa_handler = SIG_DFL;
	sigemptyset(&defaulthndl.sa_mask);
	defaulthndl.sa_flags = 0;

	if ((sigaction(SIGINT, &jumphdl, NULL) < 0) ||
	    (sigaction(SIGQUIT, &jumphdl, NULL) < 0))  {
		perror("epic fail installing signal handler");
		exit(1);
	}

	for (;;) {
		// set jmp for signals
		if (sigsetjmp(jump_to_prompt, 1))
			fputs(NEWLINE_STRING, stdout);
		jump_ok = 1;
		display_prompt();
		if (fgets( inbuf, MAX_CANON, stdin) == NULL) 
			break;
		if (*(inbuf + strlen(inbuf) - 1) == NEWLINE_SYMBOL)
			*(inbuf + strlen(inbuf) - 1) = '\0';
		if (strcmp(inbuf, QUIT_STRING) == 0)
			break;
		// changing dirs capability
		if (strstr(inbuf, "cd") != NULL) {
			if (strcmp("cd", strtok(inbuf, " ")) == 0)
				if (chdir(strtok(NULL, " ")) == -1)
						perror("failed changing directories");
		}
		else {
			if ((backp = strchr(inbuf, BACK_SYMBOL)) == NULL)
				inbackground = FALSE;
			else {
				inbackground = TRUE;
				*(backp) = NULL_SYMBOL;
			}
			sigprocmask(SIG_BLOCK, &blockmask, NULL);
			if ((child_pid = fork()) == 0) {
				if ((sigaction(SIGINT, &defaulthndl, NULL) < 0) ||
				    (sigaction(SIGQUIT, &defaulthndl, NULL) < 0)) {
					perror("failed in restoring signals in default handler");
					exit(1);
				}
				if (inbackground)
					if (setpgid(getpid(), getpid()) == -1) exit(1);
				sigprocmask(SIG_UNBLOCK, &blockmask, NULL);
				executecmdline(inbuf);
				exit(1);
			}
			else if (child_pid > 0) {
				if (!inbackground)
					while((wait_pid = waitpid(-1, NULL, 0)) > 0)
						if (wait_pid == child_pid) break;
				while (waitpid(-1, NULL, WNOHANG) > 0)
					;
			}
			sigprocmask(SIG_UNBLOCK, &blockmask, NULL);
		}
	}
}
		


static void jumphand(int signalnum) {
	if (!jump_ok) return;
	jump_ok = 0;
	siglongjmp(jump_to_prompt, 1);
}
