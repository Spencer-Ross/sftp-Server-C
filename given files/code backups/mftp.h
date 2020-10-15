#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define BACKLOG 			4
#define MY_PORT_NUMBER 		49999
#define MY_PORT_NUMBER_S 	"49999"
#define BUFF_SIZE			1024
#define ARG_LIMIT			3
#define TRUE				1
#define FALSE				0
