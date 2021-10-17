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
#include "queue.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#define PORT_NUMBER "9000"
#define BACKLOG_CONNECTIONS 6
#define MAXDATASIZE 500 // max number of bytes we can get at once
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define TIMER_BUFFER_SIZE 100

int socket_fd, client_fd, write_fd;
struct addrinfo hints, *res;
char ipstr[INET6_ADDRSTRLEN];
uint8_t daemon_arg = 0;
int terminate = 0;
//slist_data_t *datap=NULL;


// Used the following thread details struct from previous assignment
typedef struct thread_data{

    pthread_t thread_index;
    int client_fd;
    char *buffer;
    char *transmit_buffer;
    sigset_t signal_mask;
    bool thread_complete_success;
    pthread_mutex_t *mutex;
}thread_data_t;

pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct slist_data_s slist_data_t;

// slist strcuture
struct slist_data_s {
    thread_data_t thread_parameters;
    SLIST_ENTRY(slist_data_s) entries;
};

slist_data_t *datap=NULL;

SLIST_HEAD(slisthead, slist_data_s) head;


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



void close_all()
{

    terminate = 1;
    close(socket_fd);
    close(write_fd);
    closelog();
    remove(FILE_PATH);
    //cancel threads which are not completed and free associated pointers
    SLIST_FOREACH(datap,&head,entries)
    {
        if (datap->thread_parameters.thread_complete_success != true)
	{
 	    pthread_cancel(datap->thread_parameters.thread_index);
            free(datap->thread_parameters.buffer);
            free(datap->thread_parameters.transmit_buffer);
       	}
    }

    int rc  = pthread_mutex_destroy(&main_mutex);
    if(rc != 0)
    {
	printf("failed in destroying mutex\n");
    }


    // free all linked list nodes
    while(!SLIST_EMPTY(&head))
    {
        datap = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head,entries);
        free(datap);
    }
}


/* handler for SIGINT and SIGTERM */
static void signal_handler (int signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
	syslog(LOG_NOTICE, "Caught signal, exiting\n");
	terminate = 1;
	close(write_fd);
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


void receive_send_data(void* thread_param)
{

	thread_data_t *thread_func_args = (thread_data_t*)thread_param;

	int received_bytes, write_bytes, read_bytes, send_bytes;
	int buf_loc = 0;
	int realloc_count = 1;

        thread_func_args->buffer = (char *)malloc(MAXDATASIZE*sizeof(char));
        if(thread_func_args->buffer == NULL)
        {
                syslog(LOG_ERR, "thread_func_args malloc error\n");
                close_all();
        }

        thread_func_args->transmit_buffer = (char *)malloc(MAXDATASIZE*sizeof(char));
        if(thread_func_args->transmit_buffer == NULL)
        {
                syslog(LOG_ERR, "thread_func_args malloc error\n");
                close_all();
        }



	//data receive from client
	do
	{
                received_bytes = recv(thread_func_args->client_fd, thread_func_args->buffer + buf_loc, MAXDATASIZE, 0); //sizeof(buffer)
                if(received_bytes <= 0)
                {
                        syslog(LOG_ERR,"Error receiving\n");
                }

                buf_loc += received_bytes;

                // increase buffer size dynamically if required
                realloc_count += 1;
                thread_func_args->buffer = (char *)realloc(thread_func_args->buffer, realloc_count*MAXDATASIZE*(sizeof(char)));
                if(thread_func_args->buffer == NULL)
                {
                        close(thread_func_args->client_fd);
                        close_all();
                        syslog(LOG_ERR, "Realloc error\n");
			exit(EXIT_FAILURE);
                }


	}while(strchr(thread_func_args->buffer, '\n') == NULL);
	received_bytes = buf_loc;
	thread_func_args->buffer[received_bytes] = '\0';
	//total_bytes += received_bytes; //keep track of total bytes

	int rc;
	rc = pthread_mutex_lock(&main_mutex); //lock mutex
	if(rc != 0)
	{
		syslog(LOG_ERR,"Mutex lock failure\n");
		close_all();
		exit (EXIT_FAILURE);
	}

	// block the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_BLOCK, &thread_func_args->signal_mask, NULL);
	if(rc == -1)
	{
		printf("Error while blocking the signals SIGINT, SIGTERM");
		close_all();
		exit (EXIT_FAILURE);
	}

	// write to the file
	write_bytes = write(write_fd, thread_func_args->buffer, received_bytes);
	if(write_bytes < 0)
	{
		printf("Error in writing bytes\n");
		close(thread_func_args->client_fd);
		close_all();
		exit (EXIT_FAILURE);
	}

	rc = pthread_mutex_unlock(&main_mutex); // unlock mutex
	if(rc != 0)
	{
		perror("Mutex unlock failed\n");
		close_all();
		exit (EXIT_FAILURE);
	}

	// unblock the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_UNBLOCK, &thread_func_args->signal_mask, NULL);
	if(rc == -1)
	{
		printf("Error while unblocking the signals SIGINT, SIGTERM");
		close_all();
		exit (EXIT_FAILURE);
	}

	int size  = lseek(write_fd,0,SEEK_END);
	thread_func_args->transmit_buffer = (char *)realloc(thread_func_args->transmit_buffer, size*sizeof(char));
	if(thread_func_args->transmit_buffer == NULL)
	{
		close(thread_func_args->client_fd);
		printf("Error while doing realloc\n");
		close_all();
		exit (EXIT_FAILURE);
	}

	rc = pthread_mutex_lock(&main_mutex); //lock mutex
	if(rc != 0)
	{
		perror("Mutex lock failed\n");
		close_all();
		exit (EXIT_FAILURE);
	}

	// block the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_BLOCK, &thread_func_args->signal_mask, NULL);
	if(rc == -1)
	{
		printf("Error while blocking the signals SIGINT, SIGTERM");
		close_all();
		exit (EXIT_FAILURE);
	}

	lseek(write_fd, 0, SEEK_SET); //set cursor to start
	read_bytes = read(write_fd, thread_func_args->transmit_buffer, size); //read bytes
	if(read_bytes < 0)
	{
		close(thread_func_args->client_fd);
		printf("Error in reading bytes\n");
		close_all();
		exit (EXIT_FAILURE);
	}

    send_bytes = send(thread_func_args->client_fd, thread_func_args->transmit_buffer, read_bytes, 0);//send bytes
    if(send_bytes < 0)
    {
	close(thread_func_args->client_fd);
	printf("Error in sending bytes\n");
	close_all();
	exit (EXIT_FAILURE);
    }
    rc = pthread_mutex_unlock(&main_mutex); // unlock mutex
    if(rc != 0)
    {
	perror("Mutex unlock failed\n");
	close_all();
	exit (EXIT_FAILURE);
    }
    // unblock the signals SIGINT and SIGTERM
    rc = sigprocmask(SIG_UNBLOCK, &thread_func_args->signal_mask, NULL);
    if(rc == -1)
    {
	printf("Error while unblocking the signals SIGINT, SIGTERM");
	close_all();
	exit (EXIT_FAILURE);
    }
    // close connection
    close(thread_func_args->client_fd);
    //syslog(LOG_DEBUG, "Closed connection from %s", ipstr);
    free(thread_func_args->buffer);
    free(thread_func_args->transmit_buffer);
    thread_func_args->thread_complete_success = true;

}

//add timespecs and store in result
static void timespec_add(const struct timespec *ts_a, const struct timespec *ts_b, struct timespec *result)
{
    result->tv_sec = ts_a->tv_sec + ts_b->tv_sec;
    result->tv_nsec = ts_a->tv_nsec + ts_b->tv_nsec;
    if( result->tv_nsec > 1000000000L )
    {
       	result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}



//service every 10 seconds
static void timer10sec_thread(union sigval sigval)
{
    char time_buffer[TIMER_BUFFER_SIZE];
    time_t current_time;
    struct tm *timer_info;
    time(&current_time);
    timer_info = localtime(&current_time);

    int timer_buffer_size = strftime(time_buffer,TIMER_BUFFER_SIZE,"timestamp:%a, %d %b %Y %T %z\n",timer_info);

    pthread_mutex_lock(&main_mutex);

    // write to file
    int timer_writebytes = write(write_fd,time_buffer,timer_buffer_size);
    if(timer_writebytes == -1)
    {
        printf("Error in writing time to file\n");
        close_all();
        exit(-1);
    }

    pthread_mutex_unlock(&main_mutex);

}

int main(int argc, char *argv[])
{

    openlog("aesdsocket.c", LOG_PID, LOG_USER);
    // Register signals with handler
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    int rc;
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
    sigset_t signal_set;
    rc = sigemptyset(&signal_set);
    if(rc == -1)
    {
	printf("Error while doing sigemptyset()");
	return -1;
    }
    rc = sigaddset(&signal_set, SIGINT);
    if(rc == -1)
    {
        printf("Error while adding SIGINT to sigaddset()");
        return -1;
    }

    rc = sigaddset(&signal_set, SIGTERM);
    if(rc == -1)
    {
        printf("Error while adding SIGTERM to sigaddset()");
        return -1;
    }
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

    // setup timer

    struct sigevent sev;

    memset(&sev,0,sizeof(struct sigevent));

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer10sec_thread;
    timer_t timer_id;

    rc = timer_create(CLOCK_MONOTONIC,&sev,&timer_id);
    if(rc == -1)
    {
        printf("Error in timer_create()\n");
        close_all();
    }
    struct timespec start_time;

    rc = clock_gettime(CLOCK_MONOTONIC,&start_time);
    if(rc == -1)
    {
        printf("Error in clock_gettime()\n");
        close_all();
    }

    struct itimerspec itimerspec;
    itimerspec.it_interval.tv_sec = 10;
    itimerspec.it_interval.tv_nsec = 0;

    timespec_add(&start_time, &itimerspec.it_interval, &itimerspec.it_value);

    rc = timer_settime(timer_id, TIMER_ABSTIME, &itimerspec, NULL);
    if(rc == -1 )
    {
       	printf("Error in timer_settime()\n");
        close_all();
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
    //int check_tot = 0;
    //int realloc_count = 1;


    char *buf;

    buf = (char *)malloc(sizeof(char) * MAXDATASIZE);


    char * write_buf;
    write_buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

    write_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);

    if (write_fd == -1)
    {
        //printf("2\n");
        syslog(LOG_ERR, "Error in opening file");
        close(socket_fd);
        closelog();
        free(buf);
        free(write_buf);
        return -1;
    }


    SLIST_INIT(&head);

    while(!terminate)
    {

	client_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);

	if(client_fd == -1)
	{
	//	printf("1\n");
	    syslog(LOG_ERR, "Error in retrieving client fd\n");
            close(socket_fd);
            closelog();
	    return -1;
	}

	inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof ipstr);
        syslog(LOG_DEBUG,"Accepted connection from %s", ipstr);

	datap = (slist_data_t*)malloc(sizeof(slist_data_t));
	if(datap == NULL)
	{
	    printf("Error while doing malloc\n");
            close(socket_fd);
            close(write_fd);
            closelog();
            remove(FILE_PATH);
            return -1;
	}


	SLIST_INSERT_HEAD(&head, datap, entries);
	datap->thread_parameters.client_fd = client_fd;
	datap->thread_parameters.thread_complete_success = false;
	datap->thread_parameters.signal_mask = signal_set;
	datap->thread_parameters.mutex = &main_mutex;
	//int numbytes = 0;

	rc = pthread_create(&(datap->thread_parameters.thread_index), NULL, (void *)&receive_send_data, (void *) &(datap->thread_parameters)); // create pthread
	if(rc != 0)
	{
	    printf("Error while creating pthread\n");
            close_all();
            return -1;
	}

	SLIST_FOREACH(datap, &head, entries)
	{
     	    if(datap->thread_parameters.thread_complete_success == true)
	    {
		pthread_join(datap->thread_parameters.thread_index, NULL); // join threads if completed
	    }
    	}
    }
    return 0;
}
