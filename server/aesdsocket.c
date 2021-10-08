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
#define MAXDATASIZE 100 // max number of bytes we can get at once
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



    sigset_t set;
    sigemptyset(&set);           // empty the set

    // add the signals to the set
    sigaddset(&set,SIGINT);
    sigaddset(&set,SIGTERM);

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
    while(1)
    {

	if (sigprocmask(SIG_BLOCK,&set,NULL) == -1){
            perror("\nERROR sigprocmask():");
            exit(-1);
    	}

	client_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);

	if(client_fd == -1)
	{
	    syslog(LOG_ERR, "Error in retrieving client fd\n");
            close(socket_fd);
            closelog();
	    return -1;
	}

	inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof ipstr);
        syslog(LOG_DEBUG,"Accepted connection from %s", ipstr);
	int numbytes = 0;

    	char *buf;

    	buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

    	write_file_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);

    	if (write_file_fd == -1)
    	{
            syslog(LOG_ERR, "Error in opening file");
            close(socket_fd);
            closelog();
	    free(buf);
	    return -1;
    	}


	do
	{
	    numbytes = recv(client_fd, buf, MAXDATASIZE, 0);
	    if(numbytes == -1) {
        	perror("recv");
		close(socket_fd);
		closelog();
		free(buf);
        	return -1;
    	    } else {
			check_tot +=numbytes;
			int wr;
		        wr = write(write_file_fd, buf, numbytes);

        		if(wr == -1)
        		{
            		    syslog(LOG_ERR, "Error in writing to file");
			    close(socket_fd);
			    closelog();
			    free(buf);
			    return -1;
        		}

	    }
	} while(strchr(buf, '\n') == NULL);

	buf[check_tot] = '\0';

	// Set cursor to beginning of file for reading
	lseek(write_file_fd, 0, SEEK_SET);

	// buffer used for reading from file and sending through socket
	char * write_buf;
	write_buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

	int send_bytes_check = 0;

	// Read and send bytes in batches/packets of MAXDATASIZE
	while(send_bytes_check < check_tot || strchr(write_buf, '\n'))
	{
	    // seek the cursor read after the prev size of read
	    lseek(write_file_fd, send_bytes_check, SEEK_SET);
	    int read_byte = read(write_file_fd,write_buf, MAXDATASIZE);

	    // Send the bytes that were just read from file
	    send_bytes_check += read_byte;
            if (send(client_fd, write_buf,read_byte, 0) == -1)
	    {
                perror("send");
		close(socket_fd);
		closelog();
		free(buf);
		free(write_buf);
		return -1;
	    }
	}


        if (sigprocmask(SIG_UNBLOCK,&set,NULL) == -1){
            perror("\nERROR sigprocmask():");
            exit(-1);
    	}

	close(client_fd);
	free(buf);
	free(write_buf);
	close(write_file_fd);
    }
}
