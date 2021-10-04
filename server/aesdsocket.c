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
#include <ctype.h>

#define PORT_NUMBER "9000"
#define BACKLOG_CONNECTIONS 6
#define MAXDATASIZE 500 // max number of bytes we can get at once
#define FILE_PATH "/var/tmp/aesdsocketdata"

int socket_fd, client_fd, write_file_fd;
struct addrinfo hints, *res;
char ipstr[INET6_ADDRSTRLEN];
uint8_t daemon_arg = 0;

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

    int opt;
    while ((opt = getopt(argc, argv,"d")) != -1) {
        switch (opt) {
            case 'd' :
                daemon_arg = 1;
                break;
            default:
                break;
        }
    }

    int reuse_addr =1;

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
	syslog(LOG_ERR, "Error in capturing socket file descriptor\n");
	closelog();
	return -1;
    }

       // Reuse address
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1) {
        syslog(LOG_ERR, "setsockopt");
        close(socket_fd);
	closelog();
        return -1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me


    if(getaddrinfo(NULL, PORT_NUMBER, &hints, &res) != 0)
    {
	syslog(LOG_ERR, "Error in capturing address info\n");
	close(socket_fd);
	closelog();
	return -1;
    }

    if(bind(socket_fd, res->ai_addr, res->ai_addrlen) == -1)
    {
	syslog(LOG_ERR, "Error in binding\n");
	close(socket_fd);
	closelog();
	return -1;
    }

    freeaddrinfo(res); // free the linked list

    if(daemon_arg)
    {
    	int pid = fork();

    	if (pid < 0 )
	    syslog(LOG_ERR, "Forking error\n");

    	if(pid > 0)
    	{
	    syslog(LOG_DEBUG, "Daemon created\n");
	    exit(EXIT_SUCCESS);
    	}
    	if(pid == 0)
    	{
	    int sid = setsid();

            if(sid == -1)
	    {
	    	syslog(LOG_ERR, "Error in setting sid\n");
		close(socket_fd);
		closelog();
	    	exit(EXIT_FAILURE);
	    }

            if (chdir("/") == -1)
	    {
            	syslog(LOG_ERR, "chdir");
            	close(socket_fd);
		closelog();
            	exit(EXIT_FAILURE);
            }

    	    int dev_null_fd = open("/dev/null", O_RDWR);
            dup2(dev_null_fd, STDIN_FILENO);
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
    	}
    }
    if(listen(socket_fd,BACKLOG_CONNECTIONS) == -1)
    {
	syslog(LOG_ERR, "Error in listen\n");
        close(socket_fd);
        closelog();

	return -1;
    }

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    addr_size = sizeof(their_addr);

    // This variable contains total bytes transferred over all connections
    int check_tot = 0;
    int realloc_count = 1;


        char *buf;

        buf = (char *)malloc(sizeof(char) * MAXDATASIZE);


        char * write_buf;
        write_buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

	write_file_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);

        if (write_file_fd == -1)
        {
        printf("2\n");
            syslog(LOG_ERR, "Error in opening file");
            close(socket_fd);
            closelog();
            free(buf);
            free(write_buf);
            return -1;
        }




    while(1)
    {

	client_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);

	if(client_fd == -1)
	{
		printf("1\n");
	    syslog(LOG_ERR, "Error in retrieving client fd\n");
            close(socket_fd);
            closelog();
	    return -1;
	}

	inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof ipstr);
        syslog(LOG_DEBUG,"Accepted connection from %s", ipstr);
	int numbytes = 0;
/*
    	char *buf;

    	buf = (char *)malloc(sizeof(char) * MAXDATASIZE);


        char * write_buf;
        write_buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

*/


/*    	write_file_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);

    	if (write_file_fd == -1)
    	{
	printf("2\n");
            syslog(LOG_ERR, "Error in opening file");
            close(socket_fd);
            closelog();
	    free(buf);
	    free(write_buf);
	    return -1;
    	}*/

	int buf_location = 0;

	do //while((numbytes = recv(client_fd, buf + buf_location, MAXDATASIZE -1, 0)) > 0)
	{
	    numbytes = recv(client_fd, buf + buf_location, MAXDATASIZE, 0);
	    if(numbytes == -1) {
printf("3\n");
        	perror("recv");
		close(socket_fd);
		closelog();
		free(buf);
		free(write_buf);
        	return -1;
    	    }
	    ++realloc_count;
	    buf_location += numbytes;
            check_tot +=numbytes;
	    //char * temp_buf;
            buf = (char *)realloc(buf, realloc_count*MAXDATASIZE*(sizeof(char)));
	    if(buf == NULL)
	    {
printf("4\n");
            	syslog(LOG_ERR, "Error in realloc");
            	close(socket_fd);
            	closelog();
            	free(buf);
		free(write_buf);
            	return -1;
       	     }
//	     buf = temp_buf;


	//	if(strchr(buf, '\n') != NULL) break;
	} while(strchr(buf, '\n') == NULL);
	buf[buf_location] = '\0';
	//buf[check_tot] = '\0';

        int wr;
        wr = write(write_file_fd, buf, buf_location);
        if(wr == -1)
        {
printf("5\n");
            syslog(LOG_ERR, "Error in writing to file");
            close(socket_fd);
            closelog();
            free(buf);
	    free(write_buf);
            return -1;
        }

	// Set cursor to beginning of file for reading
	//lseek(write_file_fd, 0, SEEK_SET);

	// buffer used for reading from file and sending through socket
	//int send_bytes_check = 0;
	//char * temp_write_buf;
	write_buf = (char *) realloc(write_buf, check_tot*(sizeof(char)));
        if(write_buf == NULL)
        {
printf("6\n");
            syslog(LOG_ERR, "Error in realloc");
            close(socket_fd);
            closelog();
            free(buf);
            free(write_buf);
            return -1;
         }

	//write_buf = temp_write_buf;
	// Read and send bytes in batches/packets of MAXDATASIZE
	//while(send_bytes_check < check_tot)
	//{
	    // seek the cursor read after the prev size of read
//	lseek(write_file_fd, send_bytes_check, SEEK_SET);
	lseek(write_file_fd, 0, SEEK_SET);
	int read_byte = read(write_file_fd,write_buf, check_tot);

	    // Send the bytes that were just read from file
	//send_bytes_check += read_byte;
        int send_bytes = send(client_fd, write_buf,read_byte, 0);
	if(send_bytes == -1)
	{
printf("7\n");
            perror("send");
	    close(socket_fd);
	    closelog();
	    free(buf);
	    free(write_buf);
	    return -1;
	}
	//}

	close(client_fd);
	//free(buf);
	//free(write_buf);
	//close(write_file_fd);
    }
    return 0;
}
