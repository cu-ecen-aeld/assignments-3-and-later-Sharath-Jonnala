#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>

#define PORT_NUMBER "9000"
#define BACKLOG_CONNECTIONS 6
#define MAXDATASIZE 2000 // max number of bytes we can get at once 
#define FILE_PATH "/var/tmp/aesdsocketdata"

int socket_fd, client_fd, write_file_fd;
struct addrinfo hints, *res;
char ipstr[INET6_ADDRSTRLEN];

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/* handler for SIGINT and SIGTERM */
static void signal_handler (int signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
	syslog(LOG_NOTICE, "Caught signal, exiting\n");
	/** TO-DO - somehow get socket fd here to shutdown */
	close(write_file_fd);
	remove(FILE_PATH);
	shutdown(socket_fd, SHUT_RDWR);
    }
    else {
	/* this should never happen */
	closelog();
	fprintf (stderr, "Unexpected signal!\n");
	exit (EXIT_FAILURE);
    }

    closelog();
    exit (EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    openlog("aesdsocket.c", LOG_PID, LOG_USER);
    // Register signals with handler
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);


    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
	syslog(LOG_ERR, "Error in capturing socket file descriptor\n");
	return -1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me


    if(getaddrinfo(NULL, PORT_NUMBER, &hints, &res) != 0)
    {
	syslog(LOG_ERR, "Error in capturing address info\n");
	return -1;
    }

    if(bind(socket_fd, res->ai_addr, res->ai_addrlen) == -1)
    {
	syslog(LOG_ERR, "Error in binding\n");
	return -1;
    }

    freeaddrinfo(res); // free the linked list

    if(listen(socket_fd,BACKLOG_CONNECTIONS) == -1)
    {
	syslog(LOG_ERR, "Error in listen\n");
	return -1;
    }


    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    addr_size = sizeof(their_addr);
    char buf[MAXDATASIZE];


        write_file_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);

        if (write_file_fd == -1)
        {
            syslog(LOG_ERR, "Error in opening file");
        }



    while(1)
    {

    // Opening file to write packets


	client_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);

	if(client_fd == -1)
	{
	    syslog(LOG_ERR, "Error in retrieving client fd\n");
	    return -1;
	}

	inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof ipstr);
        syslog(LOG_DEBUG,"Accepted connection from %s", ipstr);
	int numbytes = 0;

	do
	{
	    if ((numbytes = recv(client_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        	perror("recv");
        	return -1;
    	    }

	} while(strchr(buf, '\n') == NULL);

	buf[numbytes] = '\0';
//write
	//int buf_count = 0;
	int wr;
// seek
	wr = write(write_file_fd, buf, numbytes);

	if(wr == -1)
	{
	    syslog(LOG_ERR, "Error in writing to file");
	}

	lseek(write_file_fd, 0, SEEK_SET);

	char write_buf[MAXDATASIZE];


	int read_byte = read(write_file_fd,write_buf, MAXDATASIZE); 

//read

   	//if (close (write_file_fd) == -1)
   	//{
	 //   syslog(LOG_ERR, "Error in closing file");
   	//}
//send sic
        if (send(client_fd, write_buf,read_byte, 0) == -1)
            perror("send");

	close(client_fd);

    }
}
