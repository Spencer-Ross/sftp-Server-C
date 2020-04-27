/***********************************************************************
name: Spencer Ross
    CS360 FINAL -- mftp server
description:    This program connects to a mini ftp client and can execute 
				the following commands received from clients (up to 4):

					1. "D” Establish the data connection. The server acknowledges the 
							command on the control connection by sending an ASCII coded 
							decimal integer, representing the port number it will listen 
							on, followed by a new line character.
					2. “C<pathname>” Change directory. The server responds by executing 
							a chdir() system call with the specified path and transmitting 
							an acknowledgement to the client. If an error occurs, an 
							error message is transmitted to the client.
					3. “L” List directory. The server child process responds by 
							initiating a child process to execute the “ls - l” command. 
							It is an error to for the client to issue this command 
							unless a data connection has been previously established. 
							Any output associated with this command is transmitted 
							along the data connection to the client. The server child 
							process waits for its child (ls) process to exit, at which 
							point the data connection is closed. Following the 
							termination of the child (ls) process, the server reads 
							the control connection for additional commands. 
					4. G<pathname>” Get a file. It is an error to for the client to 
							issue this command unless a data connection has been 
							previously established. If the file cannot be opened or 
							read by the server, an error response is generated. If 
							there is no error, the server transmits the contents of 
							<pathname> to the client over the data connection and 
							closes the data connection when the last byte has been 
							transmitted.
					5. “P<pathname>” Put a file. It is an error to for the client 
							to issue this command unless a data connection has been 
							previously established. The server attempts to open the 
							last component of <pathname> for writing. It is an error 
							if the file already exists or cannot be opened. The 
							server then reads data from the data connection and 
							writes it to the opened file. The file is closed and the 
							command is complete when the server reads an EOF from the 
							data connection, implying that the client has completed 
							the transfer and has closed the data connection.
					6. “Q” Quit. This command causes the server (child) process 
							to exit normally. Before exiting, the server will 
							acknowledge the command to the client.

***********************************************************************/

/* Includes and definitions */
#include "mftp.h"

//function prototypes
void do_C(char* pathname, int fd, int pid);
void do_ls(char* pathname, int fd);
void do_file_transfer(char* path, int datafd, int io, int pid);
int data_connection(int controlfd);


/*************************Server Function*********************************************/
void server(void) {
	int listenfd, childID;
	struct sockaddr_in servAddr;
	char *pathname, buffer[BUFF_SIZE] = {0};

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0) {
		perror("Error");
		exit(1);
	}
	if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) < 0) {
		perror("Error");
		exit(1);
	}
	

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET; 
	servAddr.sin_port = htons(MY_PORT_NUMBER); 
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listenfd, (struct sockaddr*) &servAddr, sizeof(struct sockaddr_in)) < 0) {
		perror("Error");
		exit(1);
	}

	if(listen(listenfd, BACKLOG) < 0) { //Sets up a connection queue one level deep
		perror("Error");
		exit(1);
	}

	int connectfd, length = sizeof(struct sockaddr_in);
	struct sockaddr_in clientAddr;

	/*	Waits (blocks) until a connection is established by a client
		When that happens, a new descriptor for the connection is returned 
		If there is an error the result is < 0;
		The client’s address is written into the sockaddr structure
	*/
	char hostName[NI_MAXHOST]; 
	int hostEntry, connec_count = 0;
	while(1) {
		//accept clients
		connectfd = accept(listenfd,
							(struct sockaddr*) &clientAddr,
							(socklen_t*) &length);
		if(connectfd < 0) {
			perror("accept failed");
			exit(1);
		}
		printf("Parent: accepted client connection with descriptor %d\n", connectfd);

		connec_count++;
		if(!fork()) {		//child process
			childID = getpid();
			//get hostname
			hostEntry = getnameinfo((struct sockaddr*)&clientAddr,  
									sizeof(clientAddr),
									hostName,
									sizeof(hostName),
									NULL,
									0,
									NI_NUMERICSERV);
			if(hostEntry != 0) {
				fprintf(stderr, "%s\n", gai_strerror(hostEntry));
				exit(1);
			} 
			printf("Child %d: Connection accepted from host %s\n", childID, hostName);
			fflush(stdout);

			char cmnd, *err_msg = "Error no data connection\n";
			int datafd = -1;
			while(1) {
				bzero(buffer, BUFF_SIZE);
				if(read(connectfd, buffer, BUFF_SIZE) < 0) {
					perror("couldn't read from client socket\n");
				}
				cmnd = buffer[0];
				if((cmnd != 'Q') || (cmnd != 'L') || (cmnd != 'D')) 
					pathname = &buffer[1]; //grab just the part after the command char
				if(cmnd == 'Q') {
					write(connectfd, "A\n", 2);
					printf("Child %d: Quitting\n", childID);
					exit(0);
				} else if(cmnd == 'C') {
					do_C(pathname, connectfd, childID);
				} else if(cmnd == 'D') {
					datafd = data_connection(connectfd);
				} else if(cmnd == 'L') {
					if(datafd == -1) { //no data socket
						write(connectfd, err_msg, sizeof(err_msg));
						continue;
					}
					do_ls(pathname, datafd);
					close(datafd);
				} else if(cmnd == 'G') {
					if(datafd == -1) { //no data socket
						write(connectfd, err_msg, sizeof(err_msg));
						continue;
					}
					printf("io = %d\n", 1);
					do_file_transfer(pathname, datafd, 1, childID); //send 1 for output
					close(datafd);
				} else if(cmnd == 'P') {
					if(datafd == -1) { //no data socket
						write(connectfd, err_msg, sizeof(err_msg));
						continue;
					}
					printf("io = %d\n", 0);
					do_file_transfer(pathname, datafd, 0, childID); //send 0 for input
					close(datafd);
				}
			}
		} else { //parent closes and moves on
			close(connectfd);
		}
	}
}



/**********************executes "cd" command************************************************/
void do_C(char* pathname, int fd, int pid) {
	int err;
	char *err_msg = "ENo such file or directory\n";
	pathname[strlen(pathname)-1] = '\0';
	if((err = chdir(pathname)) < 0) {
		perror("cd");
		write(fd, err_msg, sizeof(err_msg));
	}
	else {
		printf("Child %d: Changed current directory to %s\n", pid, pathname);
		write(fd, "A\n", 2);
	}
	return;
}

/**********************executes "ls" command************************************************/
void do_ls(char* pathname, int fd) {
	char *ls[4] = {"ls", "-l", "-a", NULL}, buffer[BUFF_SIZE];
	bzero(buffer, BUFF_SIZE);

	// printf("forking ls process\n"); //DEBUG
	if(!fork()) {						//child: exec left of '|' to write to pipe
		if((dup2(fd, 1) < 0))
			perror("child dup2");
		if(execvp(ls[0], ls) == -1)
			perror("child execvp");
	} else {							//parent: exec right of '|'
		wait(NULL); 					//wait for child to end
	}
	return;
}

/**********************executes "get"/"show" command******************************************/
void do_file_transfer(char* path, int datafd, int io, int pid) {
	int numberRead, fd;
	char *ptr, *pathname, *delims = "\n", buffer[BUFF_SIZE];

	//send acknowledgement 
	if(write(datafd, "A\n", 2) < 0) {
			perror("read remote2");
			// exit(1);
	}

	ptr 	 = strdup(path);
	pathname = strtok(ptr, delims);
	printf("pathname=%s\n", pathname);

	if((fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0755)) < 0)
		perror("Error open or create");
	
	if(!io) {
		printf("Child %d: Writing file %s\n", pid, pathname);
		while((numberRead = read(datafd, &buffer, BUFF_SIZE)) > 0) {
			if(write(fd, buffer, numberRead) < 0) {
				perror("Error writing to file");
			}
		}
		if(numberRead < 0) {
			perror("Error reading from data socket");
		}
		printf("Child %d: receiving file %s from client\n", pid, pathname);
	} else {
		printf("Child %d: reading file %s\n", pid, pathname);
		while((numberRead = read(fd, &buffer, BUFF_SIZE)) > 0) {
			if(write(datafd, buffer, numberRead) < 0) {
				perror("Error writing file to data socket");
			}
		}
		if(numberRead < 0) {
			perror("Error reading from file");
		}
		printf("Child %d: transmitting file %s to client\n", pid, pathname);
	}
	close(fd);
	bzero(buffer, BUFF_SIZE);
	return;
}

/**********************opens data connection for commands*************************************/
int data_connection(int controlfd) {
	struct sockaddr_in servAddr, newServAddr, clientAddr;
	int listenfd, datafd, sockSize = sizeof(newServAddr);
	char buffer[BUFF_SIZE];
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	// printf("data socket created with descriptor\n"); //DEBUG
	if(listenfd < 0) {
		perror("unable to listen to socket in data_connection");
		exit(1);
	}
	if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) < 0) {
		perror("Error setsockopt");
		exit(1);
	}

	memset(&servAddr, 0, sizeof(servAddr));
	memset(&newServAddr, 0, sizeof(newServAddr));	
	servAddr.sin_family = AF_INET; 
	servAddr.sin_port = htons(0); 
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(listenfd, (struct sockaddr*) &servAddr, sizeof(struct sockaddr_in)) < 0) {
		perror("tried to bind in data_connection");
		exit(1);
	}
	
	if(getsockname(listenfd, (struct sockaddr*) &newServAddr, (socklen_t*) &sockSize) < 0) {
		perror("couldn't get socket name");
		exit(1);
	}
	// printf("data socket bound to port %d\n", htons(newServAddr.sin_port)); //DEBUG
	sprintf(buffer, "A%d\n", htons(newServAddr.sin_port));
	// printf("Sending acknowledgement -> %s\n", buffer); //DEBUG
	if(write(controlfd, buffer, BUFF_SIZE) < 0) {
		perror("Couldn't write acknowledgement");
		exit(1);
	}	

	sockSize = sizeof(struct sockaddr_in);
	if(listen(listenfd, BACKLOG) < 0) { 
		perror("data_connection listen");
		exit(1);
	}
	if((datafd = accept(listenfd, 
						(struct sockaddr*) &clientAddr, 
						(socklen_t*) &sockSize)) < 0) {
		perror("data connection accept failed");
		exit(1);
	}

	// char hostName[NI_MAXHOST]; //DEBUG
	// int hostEntry;				//DEBUG
	// hostEntry = getnameinfo((struct sockaddr*)&clientAddr,  //DEBUG
									// sizeof(clientAddr),		//DEBUG
									// hostName,				//DEBUG
									// sizeof(hostName),		//DEBUG
									// NULL,					//DEBUG
									// 0,						//DEBUG
									// NI_NUMERICSERV);		//DEBUG
	// if(hostEntry != 0) {									//DEBUG
		// fprintf(stderr, "%s\n", gai_strerror(hostEntry));	//DEBUG
		// exit(1);											//DEBUG
	// } 
	// printf("Accepted connection from host %s on the data socket with descriptor %d\n", hostName, datafd); //DEBUG
	// printf("Data socket port number on the client end is %d\n", htons(clientAddr.sin_port)); //DEBUG
	// printf("Sending positive acknowledgement\n");											//DEBUG
	if(write(controlfd, "A\n", 2) < 0) {
		perror("Error sending acknowledgement");
		exit(1);
	}
	close(listenfd);
	return datafd;
}


/***********************************************************************/
int main(int argc, char const *argv[]){
	server();
    return 0;
}
