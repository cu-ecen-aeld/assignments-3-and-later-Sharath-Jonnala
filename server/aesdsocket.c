#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <syslog.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define PORT "9000"
#define MAX_CONNECTIONS 9
#define BUFFER_SIZE 500
#define FILE_PATH "/var/tmp/aesdsocketdata"

int socket_fd, client_fd, write_fd;
char *buffer, *transmit_buffer;

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
        if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_DEBUG, "Caught Signal, exiting");
		close(socket_fd);
		close(client_fd);
	        close(write_fd);
		closelog();
		free(buffer);
		free(transmit_buffer);
		remove(FILE_PATH);
	}
        else
       	{
                /* this should never happen */
                printf ("Unexpected signal!\n");
                exit (EXIT_FAILURE);
        }
	
        exit (EXIT_SUCCESS);
}


int main(int argc, char*argv[])
{
        int rc;
	int received_bytes, write_bytes, send_bytes, read_bytes;
	int loc = 0, count = 1;
	int total_bytes = 0;
	int enable_daemon = 0;
	char ipstr[INET6_ADDRSTRLEN];
	struct addrinfo hints, *res;
	struct sockaddr_storage their_addr;
        socklen_t addr_size;
	openlog("aesd-socket-log", LOG_PID, LOG_USER);
        
	int opt;
    	while ((opt = getopt(argc, argv,"d")) != -1)
       	{
       	        switch (opt)
       		{
            		case 'd':
                		enable_daemon = 1;
				printf("enabled daemon\n");
				break;
            		default:
				break;
       	        }
    	}

	// Register signal_handler as our signal handler for SIGINT
        if (signal (SIGINT, signal_handler) == SIG_ERR)
       	{
                printf("Cannot handle SIGINT!\n");
                exit (EXIT_FAILURE);
        }
        
	// Register signal_handler as our signal handler for SIGTERM
        if (signal (SIGTERM, signal_handler) == SIG_ERR)
       	{
                printf("Cannot handle SIGTERM!\n");
                exit (EXIT_FAILURE);
        }

	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
        
	if(socket_fd < 0)
	{
		printf("Error while creating socket\n");
		return -1;
	}
        
	int reuse_addr = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1)
       	{
       		 syslog(LOG_ERR, "setsockopt");
       		 close(socket_fd);
       		 return -1;
        }

	memset(&hints, 0 , sizeof (hints));

	// load address struct with getaddrinfo
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(NULL, PORT, &hints, &res);

	// bind socket to the port we passes in getaddrinfo()
        rc = bind(socket_fd, res->ai_addr, res->ai_addrlen);
        if(rc < 0)
	{
		printf("Error while binding the socket\n");
		return -1;
	}

	freeaddrinfo(res);

	if(enable_daemon == 1)
	{

		pid_t pid = fork();

       		if(pid < 0 )
		{
           		syslog(LOG_ERR, "Forking error\n");
		}
       		if(pid > 0)
       		{
           		 syslog(LOG_DEBUG, "Daemon created\n");
           		 exit(EXIT_SUCCESS);
       		}
       		if(pid == 0)
       		{
			pid_t sid = setsid();

           		if(sid < 0)
           		{
                		syslog(LOG_ERR, "Error in setting session id\n");
                		exit(EXIT_FAILURE);
           		}
			
			//change to root directory
           		if (chdir("/") == -1)
           		{
                		syslog(LOG_ERR, "could not change directory to root");
               	        	close(socket_fd);
               	        	exit(EXIT_FAILURE);
           	        }

			//redirect input, output and error
           	        int dev_null_fd = open("/dev/null", O_RDWR);
           	 	dup2(dev_null_fd, STDIN_FILENO);
           	 	dup2(dev_null_fd, STDOUT_FILENO);
           	 	dup2(dev_null_fd, STDERR_FILENO);

			//close all file descriptors
           	 	close(STDIN_FILENO);
           	 	close(STDOUT_FILENO);
           	 	close(STDERR_FILENO);
		}
        }

	
	//listen
        rc = listen(socket_fd, MAX_CONNECTIONS);
        
	if(rc < 0)
	{
		printf("Error while listening to socket\n");
		return -1;
	}

        addr_size = sizeof(their_addr);  
        
        // open file at FILE_PATH
        write_fd = open(FILE_PATH, O_CREAT | O_RDWR | O_APPEND, 0644);
        if(write_fd < 0)
        {
               syslog(LOG_ERR, "Error: file specified as argument couldn't be opened(write_fd < 0)");
               return -1;
        }
	
        // allocate memory for buffers
	buffer = (char *)malloc(BUFFER_SIZE*sizeof(char));
	if(buffer == NULL)
	{
		printf("Error while doing malloc\n");
		return -1;
	}

	transmit_buffer = (char *)malloc(BUFFER_SIZE*sizeof(char));
        if(transmit_buffer == NULL)
        {
                printf("Error while doing malloc\n");
                return -1;
        }

	while(1)
	{
	        // accept an incomming connection
                client_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size); 
                if(client_fd < 0)
	        {
			printf("Error while accpeting incomming connection\n");
			close(socket_fd);
                	close(write_fd);
                	closelog();
        	        free(buffer);
	                free(transmit_buffer);
	                remove(FILE_PATH);
			return -1;
	        }
        
	        // convert the IP to a string and print it:
                inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof ipstr);
                syslog(LOG_DEBUG,"Accepted connection from %s", ipstr);

		loc = 0;
		
        	// receive data
		do
		{
	       		received_bytes = recv(client_fd, buffer+loc, BUFFER_SIZE, 0); //sizeof(buffer)
       			if(received_bytes <= 0)
			{
				printf("Error while receiving\n");
			}
		
			loc += received_bytes;
			
			// increase buffer size dynamically if required
			count += 1;
			buffer = (char *)realloc(buffer, count*BUFFER_SIZE*(sizeof(char)));
		        if(buffer == NULL)
			{
				close(socket_fd);
	                	close(client_fd);
        	        	close(write_fd);
                		closelog();
                		free(buffer);
                		free(transmit_buffer);
                		remove(FILE_PATH);
				printf("Error while doing realloc\n");
				return -1;
			}
				

		}while(strchr(buffer, '\n') == NULL);
		received_bytes = loc;
         	buffer[received_bytes] = '\0';
		total_bytes += received_bytes; //keep track of total bytes 
	
		
		// write to the file
	        write_bytes = write(write_fd, buffer, received_bytes);
                if(write_bytes < 0)
		{
			printf("Error in writing bytes\n");
			close(socket_fd);
                        close(client_fd);
                        close(write_fd);
                        closelog();
                        free(buffer);
                        free(transmit_buffer);
                        remove(FILE_PATH);
			return -1;
		}
		
		transmit_buffer = (char *)realloc(transmit_buffer, total_bytes*sizeof(char));
                if(transmit_buffer == NULL)
                {
			close(socket_fd);
                        close(client_fd);
                        close(write_fd);
                        closelog();
                        free(buffer);
                        free(transmit_buffer);
                        remove(FILE_PATH);
                        printf("Error while doing realloc\n");
                        return -1;
                }
              

		lseek(write_fd, 0, SEEK_SET); //set cursor to start
		read_bytes = read(write_fd, transmit_buffer, total_bytes); //read bytes
		if(read_bytes < 0)
		{
			close(socket_fd);
                        close(client_fd);
                        close(write_fd);
                        closelog();
                        free(buffer);
                        free(transmit_buffer);
                        remove(FILE_PATH);
			printf("Error in reading bytes\n");
			return -1;
		}

	        send_bytes = send(client_fd, transmit_buffer, read_bytes, 0);//send bytes
                if(send_bytes < 0)
                {
			close(socket_fd);
                        close(client_fd);
                        close(write_fd);
                        closelog();
                        free(buffer);
                        free(transmit_buffer);
                        remove(FILE_PATH);
                        printf("Error in sending bytes\n");
			return -1;
                }


		// close connection
		close(client_fd);
		syslog(LOG_DEBUG, "Closed connection from %s", ipstr);
                
	}
	
        return 0;


}
