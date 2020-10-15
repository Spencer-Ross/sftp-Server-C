/***********************************************************************
name: Spencer Ross
    CS360 FINAL -- mftp client
description:    This program connects to a mini ftp server and can execute 
				the following commands:

					1. exit: terminate the client program after 
							instructing the server’s child process to do 
							the same.
					2. cd <pathname>: change the current working directory 
							of the client process to <pathname>. 
					3. rcd <pathname>: change the current working directory 
							of the server child process to <pathname>. 
					4. ls: execute the “ls –l | more” command locally and display 
							the output to standard output, 20 lines at a time, 
							waiting for a space character on standard input 
							before displaying the next 20 lines. 
					5. rls: execute the “ls –l -a” command on the remote server 
							and display the output to the client’s standard 
							output, 20 lines at a time in the same manner as 
							“ls” above.
					6. get <pathname>: retrieve <pathname> from the server 
							and store it locally in the client’s current 
							working directory using the last component of 
							<pathname> as the file name. 
					7. show <pathname>: retrieve the contents of the indicated 
							remote <pathname> and write it to the client’s standard 
							output, 20 lines at a time same manner as “ls” above.
					8. put <pathname>: transmit the contents of the local file 
							<pathname> to the server and store the contents in the 
							server process’ current working directory using the 
							last component of <pathname> as the file name. 

***********************************************************************/

/* Includes and definitions */
#include "mftp.h"

//function prototypes
void quit(int socketfd, char* hostname);
void execute_cd(char* pathname);
void exec_ls(void);
void execute_rls(int socketfd, char* hostname);
void exec_remote(int socketfd, char* hostname, char* pathname, char ctrl_cmnd, int gf);
void pipe_fn(char** left, char** right);
int get_socket(char* address, char* port);
void remote(int socketfd, char* hostname, char* cmnd);
void move_file(int socketfd, char* hostname, char* cmnd, char* pathname);

/*************************Client Function***************************************************/
void client(char *addr) {
	int socketfd;
	char buffer[BUFF_SIZE] = {0};
	char *cmnd, *pathname, *inputPtr, *delims = " \n"; //maybe get rid of '\n' delim

	socketfd = get_socket(addr, MY_PORT_NUMBER_S);
	printf("Connected to server %s\n", addr);

	while(1) {
		bzero(buffer, BUFF_SIZE);	//clear out buffer
		//get user input
		printf("MFTP> ");
		fflush(stdout);
		if(read(0, &buffer, BUFF_SIZE) < 0) {
			perror("read stdin");
			exit(1);
		}

		//parse tokens
		inputPtr = strdup(buffer);
		cmnd 	 = strtok(inputPtr, delims);
		pathname = strtok(NULL, delims);

		//run through and check cmnd then execute command
		if(cmnd == NULL) continue;
		if(strcmp(cmnd, "exit") == 0) 
			quit(socketfd, addr);
		else if(strcmp(cmnd, "cd") == 0) 
			execute_cd(pathname);
		else if(strcmp(cmnd, "rcd") == 0) 
			exec_remote(socketfd, addr, pathname, 'C', FALSE);
		else if(strcmp(cmnd, "ls") == 0) 
			exec_ls();
		else if(strcmp(cmnd, "rls") == 0) 
			execute_rls(socketfd, addr);
		else if(strcmp(cmnd, "get") == 0)
			exec_remote(socketfd, addr, pathname, 'G', TRUE);
		else if(strcmp(cmnd, "show") == 0)
			exec_remote(socketfd, addr, pathname, 'G', FALSE);
		else if(strcmp(cmnd, "put") == 0)
			exec_remote(socketfd, addr, pathname, 'P', TRUE);
		else printf("Command '%s' is unknown - ignored\n", cmnd);
	}
	return;
}


/**********************executes "cd" command************************************************/
void quit(int socketfd, char* hostname) {
	remote(socketfd, hostname, "Q\n");
	exit(0);
}

/**********************executes "cd" command************************************************/
void execute_cd(char* pathname) {
	if(chdir(pathname) < 0) 
		perror("cd");
	return;
}

/**********************prepares "ls" command and sends it to pipe_fn()**********************/
void exec_ls(void) {
	char *execL[4] = {"ls", "-l", "-a", NULL};
	char *execR[2] = {"more", NULL};
	pipe_fn(execL, execR);
	return;
}

/**********************prepares "rls" command and sends it to remote()**********************/
void execute_rls(int socketfd, char* hostname) {
	remote(socketfd, hostname, "L\n");
	return;
}

/*********prepares remote commands that use pathname and sends it to remote()***************/
void exec_remote(int socketfd, char* hostname, char* pathname, char ctrl_cmnd, int gf) {
	if(pathname == NULL) {
		printf("Command error: expecting a parameter.\n");
		fflush(stdout);
		return;
	}
	char command[strlen(pathname) + 2];
	command[0] = ctrl_cmnd;
	command[1] = '\0';
	strcat(command, pathname);
	command[strlen(command)] = '\n';
	if(gf) 
		move_file(socketfd, hostname, command, pathname);
	else 
		remote(socketfd, hostname, command);
	return;
}

/************************executes client-side commands with a pipe**************************/
void pipe_fn(char** left, char** right) {
	int fd[2];
	if(!fork()) {
		if(pipe(fd) < 0) perror("pipe");
		if(!fork()) {						//child: exec left of '|' to write to pipe
			if(close(fd[0]) < 0)			//read end is unused
				perror("child close");
			if(fd[1] != 1) {				//Defensive check -- from textbook
				if((dup2(fd[1], 1) < 0))
					perror("child dup2");
			}
			if(execvp(left[0], left) == -1) //Writes to pipe
				perror("child execvp");
		} else {							//parent: exec right of '|'
			wait(NULL); 					//wait for child to end
			if(close(fd[1]) < 0)			//write end is unused
				perror("parent close");	
			if(fd[0] != 0) {
				if((dup2(fd[0], 0) < 0))
					perror("parent dup2");
			}	
			if(execvp(right[0], right) == -1)
				perror("parent execvp");
		}
	} else {
		wait(NULL);
	}
	return;
}

/************************sends requests to server for remote commands***********************/
void remote(int socketfd, char* hostname, char* cmnd) {
	int data_socketfd, numberRead;
	char buffer[BUFF_SIZE], *more[2] = {"more", NULL};

	//if data socket not needed
	bzero(buffer, BUFF_SIZE);
	if((cmnd[0] == 'C') || (cmnd[0] == 'Q')) {
		if(write(socketfd, cmnd, strlen(cmnd)) < 0) {
			perror("Writing w/o data socket");
		}
		if(read(socketfd, &buffer, BUFF_SIZE) < 0) {
			perror("read remote2");
			// exit(1);
		}
		return;
	}

	//send make data socket
	if(write(socketfd, "D\n", 2) < 0) {
		perror("write remote");
	}
	if((numberRead = read(socketfd, &buffer, BUFF_SIZE) < 0)) {
			perror("read remote");
			// exit(1);
	}

	//getting port number of data socket
	char* port = &buffer[1];
	data_socketfd = get_socket(hostname, port); //open the socket
	bzero(buffer, BUFF_SIZE);

	//write command to server
	if(write(socketfd, cmnd, strlen(cmnd)) < 0) {
		perror("write remote2");
	}
	bzero(buffer, BUFF_SIZE);
	if(read(socketfd, &buffer, BUFF_SIZE) < 0) {
			perror("read remote2");
	}

	//read output from server
	if(!fork()) {						//child: exec left of '|' to write to pipe
		if(0 != data_socketfd) {
			if((dup2(data_socketfd, 0) < 0))
				perror("child dup2");
		}	
		if(execvp(more[0], more) == -1)
			perror("child execvp");
	} else {							//parent: exec right of '|'
		wait(NULL); 					//wait for child to end
		while(((numberRead = read(data_socketfd, &buffer, BUFF_SIZE)) > 0)) {
			if(write(1, buffer, numberRead) < 0) {
				perror("write more");
			}
		}
		if(numberRead < 0) {
			perror("read more");
		}
	}
	bzero(buffer, BUFF_SIZE);
	return;
}

/************************sends requests to server for "get"/"put" commands***********************/
void move_file(int socketfd, char* hostname, char* cmnd, char* pathname) {
	int data_socketfd, numberRead, fd;
	char buffer[BUFF_SIZE];
	//send make data socket
	if(write(socketfd, "D\n", 2) < 0) {
		perror("write remote");
	}
	if((numberRead = read(socketfd, &buffer, BUFF_SIZE) < 0)) {
			perror("read remote");
			// exit(1);
	}
	//getting port number of data socket
	char* port = &buffer[1];
	data_socketfd = get_socket(hostname, port); //open the socket
	bzero(buffer, BUFF_SIZE);
	//write command to server
	if(write(socketfd, cmnd, strlen(cmnd)) < 0) {
		perror("write remote2");
	}
	if((numberRead = read(socketfd, &buffer, BUFF_SIZE) < 0)) {
			perror("read remote2");
			// exit(1);
	}

	if(((fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0755)) < 0))
		perror("Error open or create");
	//read output from server
	if(cmnd[0] == 'G') {
		while(((numberRead = read(data_socketfd, &buffer, BUFF_SIZE)) > 0)) {
			if(write(fd, buffer, numberRead) < 0) {
				perror("Error writing to file");
			}
		}
		if(numberRead < 0) {
			perror("Error reading from data_socketfd");
		}
	} else {
		while(((numberRead = read(fd, &buffer, BUFF_SIZE)) > 0)) {
			if(write(data_socketfd, buffer, numberRead) < 0) {
				perror("Error writing file to data socket");
			}
		}
		if(numberRead < 0) {
			perror("Error reading from file");
		}
		close(data_socketfd);
	}
	close(fd);
	bzero(buffer, BUFF_SIZE);
	return;
}

/************************creates a socket connection with server****************************/
int get_socket(char* address, char* port) {
	int socketfd, err;
	struct addrinfo hints, *actualData;
	memset(&hints, 0, sizeof(hints));
	char *portMod; 

	portMod = strdup(port);
	portMod[strlen(portMod)-1] = (portMod[strlen(portMod)-1] == '\n') ? 
								'\0' : portMod[strlen(portMod)-1];
	
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family	  = AF_INET;

	err = getaddrinfo(address, 
					  portMod, 
					  &hints, 
					  &actualData);
	if(err != 0) {
		fprintf(stderr, "Host name translation failed: %s\n", gai_strerror(err));
		exit(1);
	}
	socketfd = socket(actualData->ai_family, 
					  actualData->ai_socktype,
					  0);
	if(socketfd < 0) {
		perror("Error");
		exit(1);
	}
	if(connect(socketfd, actualData->ai_addr, actualData->ai_addrlen) < 0) {
		perror("Connection to server failed");
		exit(1);
	}
	return socketfd;
}

/***************************usage message for starting program******************************/
void usrmsg(void) {
	printf("Usage: ./mftp [-d] <hostname | IP address>\n");
	return;
}

/****************Main function starts client when address token is given********************/
int main(int argc, char const *argv[]){
	if(argc > 1) {	//if address token given
		char address[strlen(argv[1]) + 1];
		address[strlen(argv[1])] = '\0';
		strcpy(address, argv[1]);
		client(address);
		return 0;
	}
	usrmsg();
    return 1;
}
