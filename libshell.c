#include"libshell.h"


// diplay_prompt
// gets the current working dir string
// and uses for prompt string for
// easier navigation
void display_prompt() {
	char current_dir[MAX_CANON];
	char PROMPT[MAX_CANON];
	getcwd(current_dir, MAX_CANON);
	sprintf(PROMPT, "\033[1;36m%s-%s\033[0m", current_dir, PROMPT_STRING);
	fputs(PROMPT, stdout);
}

// make argv array for tokens
// return -1 on error else number of tokens 
// used in executecmdline
int makeargv(char *s, char *delimiters, char ***argvp) {
	char *t;
	char *snew;
	int numtokens;
	int i;
	char *placeholder;
	
	// snew is real start of string after skipping leading delimiter
	snew = s + strspn(s, delimiters);

	// create a new space for copy of snew in t
	if ((t = calloc(strlen(snew) + 1, sizeof(char))) == NULL) {
		*argvp = NULL;
		numtokens = -1;
	} else {
		strcpy(t, snew);
		if (strtok_r(t, delimiters, &placeholder) == NULL)
			numtokens = 0;
		else
			// empty for loop
			for (numtokens = 1; strtok_r(NULL, delimiters, &placeholder) != NULL; numtokens++) 
				; 	
	// create arg array
	if ((*argvp = calloc(numtokens + 1, sizeof(char *))) == NULL) {			
			free(t);
			numtokens = -1;
		} else {
			if (numtokens > 0) {
				strcpy(t, snew);
				// fill array with pointers
				**argvp = strtok_r(t, delimiters, &placeholder);
				for (i = 1; i < numtokens + 1; i++)
					*((*argvp) + i) = strtok_r(NULL, delimiters, &placeholder);
			} else {
				**argvp = NULL;
				free(t);
			}
		}
	}
	return numtokens;
}



// function redirects stdout to outfile 
// and stdin to infile
// if either == NULL no redirection occurs
// returns 0 on victory, -1 on brutal defeat
int redirect(char *infile, char *outfile) {
	int in_fd;
	int out_fd;
	
	// redirect stdin
	if (infile != NULL) {
		if ((in_fd = open(infile, O_RDONLY, STDMODE)) == -1)
			return -1;
		if (dup2(in_fd, STDIN_FILENO) == -1) {
			close(in_fd);
			return -1;
		}
		close(in_fd);
	}

	// ... and now for stdout to fd_out
	if (outfile != NULL) {
		if ((out_fd = open(outfile, O_WRONLY|O_CREAT, STDMODE)) == -1)
			return -1;
		if (dup2(out_fd, STDOUT_FILENO) == -1) {
			close(out_fd);
			return -1;
		}
		close(out_fd);
	}
	return 0;
}


// parsefile removes token following delimiter in s.
// v points to the token on return and removes them from s	
// used in executecmdline for redirect funcctionality
int parsefile(char *s, char delimiter, char **v) {
	char *p;
	char *q;
	int offset;
	int error;
	char *strtok_placeholder;
	
	// find delimiter pos
	*v = NULL;
	if ((p = strchr(s, delimiter)) != NULL) {
		//split off the token 
		if ((q = (char*)malloc(strlen(p + 1) + 1)) == NULL)
			error = -1;
		else {
			strcpy(q, p + 1);
			if ((*v = strtok_r(q, DELIMITERSET, &strtok_placeholder)) == NULL)
				error = -1;
			offset = strlen(q);
			strcpy(p, p + offset + 1);
		}
	}
	return error;
}


// connectpipeline connects the process to a pipeline by redirecting
// stdin to frontfd[0] and stdout to backfd[1]. If frontfd[0] = -1, the process is
// at the front of the pipeline and stdin may be redirected to a file. If backfd[1] = -1, 
// the process is at the back of the pipeline and stdout may be redirected to a file.
// Otherwise redirection to a file is an error. If explicit redirection occurs in cmd,
// it is removed during processing. 0 on victory, -1 on cowardly retreat
// used in executecmdline
int connectpipeline(char *cmd, int frontfd[], int backfd[]) {
	int error = 0;
	char *infile;
	char *outfile;

	if (parsefile(cmd, IN_REDIRECT_SYMBOL, &infile) == -1)
		error = -1;
	else if (infile != NULL && frontfd[0] != -1)
		error = -1;
	else if (parsefile(cmd, OUT_REDIRECT_SYMBOL, &outfile) == -1)
		error = -1;
	else if (outfile != NULL && backfd[1] != -1)
		error = -1;
	else if (redirect(infile, outfile) == -1)
		error == -1;
	else {
		if (frontfd[0] != -1) {
			if (dup2(frontfd[0], STDIN_FILENO) == -1)
				error = -1;
		}
		if (backfd[1] != -1) {
			if (dup2(backfd[1], STDOUT_FILENO) == -1)
				error = -1;
		}
	}
	close(frontfd[0]);
	close(frontfd[1]);
	close(backfd[0]);
	close(backfd[1]);
	return error;
}


// executes command string from prompt uses makeargv redirect and parsefile and connectpipeline
void executecmdline(char *incmd) {
	char **chargv;
	pid_t child_pid;
	char *cmd;
	char *nextcmd;
	int frontfd[2];
	int backfd[2];

	
	frontfd[0] = -1;
	frontfd[1] = -1;
	backfd[0]  = -1;
	backfd[1]  = -1;

	child_pid = 0;
	
	if ((nextcmd = incmd) == NULL)
		exit(1);

	for (;;) {
		cmd = nextcmd;
		if (cmd == NULL) break;
		// if last in pipeline, don't fork
		if ((nextcmd = strchr(nextcmd, PIPE_SYMBOL)) == NULL) {
			backfd[1] = -1;
			child_pid = 0;
		} else {
			// fork child to execute next pipeline command
			*nextcmd = NULL_SYMBOL;
			nextcmd++;
			if (pipe(backfd) == -1) {
				perror("Couldn't create back pipe");
				exit(1);
			} else if ((child_pid = fork()) == -1) {
				perror("Couldn't fork next child");
				exit(1);
			}
		}

		if (child_pid == 0) {
			// child exec cmd
			if (connectpipeline(cmd, frontfd, backfd)  == -1) {
				perror("Couldn't connect pipes");
				exit(1);
			} else if (makeargv(cmd, BLANK_STRING, &chargv) > 0) {
				if (execvp(chargv[0], chargv) == -1)
					perror("Invalid command");
			}
			exit(1);
		}

		// parent closes front pipe and makes the back pipe the front
		close(frontfd[0]);
		close(frontfd[1]);
		frontfd[0] = backfd[0];
		frontfd[1] = backfd[1];
	}
	close(backfd[0]);
	close(backfd[1]);
	exit(1);
}


// gets current user info for saving histfile
char *get_user_homedir() {
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	char *homedir = NULL;
	if (pw == NULL) {
		fprintf(stderr, "Error gathering user info\n");
		exit(1);
	}
	endpwent();
	return pw->pw_dir;
}

// init_history is basically just a wrapper around fopen 
// that takes the macro HISTORYFILE as an argument and 
// stores it in the users home dir 
FILE *init_history() {
	char *user_homedir = get_user_homedir();
	char usrdir[MAX_CANON];
	if (strlen(user_homedir) > MAX_CANON) {
	       fprintf(stderr, "excessive dir string length\n");
       		exit(1);
	}		
	sprintf(usrdir, "%s/%s", user_homedir, HISTORYFILE);
	FILE *histfile = fopen(usrdir, "a+");
	if (histfile == NULL) {
		perror("error creating histfile");
		exit(1);
	}
	return histfile;
}

// conv funct to read histfile that adds a count to the hist  
void read_history(FILE *file) {
	int count;
	rewind(file);
	for (count = 1; !feof(file); count++) {
		char hist_buf[MAX_CANON];
		fscanf(file, "%[^\n]\n", hist_buf);
		fprintf(stdout, "%-3d %s\n", count, hist_buf);
	}
}

// writes hist to the hist file fprint wrap
void write_history(FILE *file, char *history) {
	fseek(file, SEEK_END, SEEK_SET);
	fprintf(file, "%s\n", history);
}


