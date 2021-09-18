#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

//int thread_index = 0;

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter

    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    //useconds_t wait_us = thread_func_args->wait_period;
    //useconds_t lock_us = thread_func_args->lock_period;
    //pthread_mutex_t mutex_local = thread_func_args->mutex;

    usleep(thread_func_args->wait_period);	// Waiting to obtain new threads

  //  if(pthread_mutex_init(thread_func_args->mutex, NULL) != 0)
   // {
//	thread_func_args->thread_complete_success = false;
//	return thread_param;
  //  }

    if(pthread_mutex_lock(thread_func_args->mutex) != 0)
    {
	ERROR_LOG("Error in locking mutex \n");
	thread_func_args->thread_complete_success = false;
	return thread_param;
    }

    usleep(thread_func_args->lock_period);

    if(pthread_mutex_unlock(thread_func_args->mutex) !=0)
    {
	ERROR_LOG("Error in unlocking mutex \n");
	thread_func_args->thread_complete_success = false;
	return thread_param;
    }

    thread_func_args->thread_complete_success = true;
    //free(thread_param);
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    //++thread_index;

    pthread_t new_thread;
    struct thread_data * val = (struct thread_data * )malloc(sizeof(struct thread_data));

    //val->thread_id = thread_index;
    val->wait_period = wait_to_obtain_ms;
    val->lock_period = wait_to_release_ms;
    val->mutex = mutex;

    int ret = pthread_create(&new_thread, NULL, threadfunc, (void *)val);

    if(ret == 0) {
	*thread = new_thread;
	return true;
    }
//    for(int i = 1; i<= thread_index; i++) {

//	pthread_join(i, NULL);
//    }

    return false;
}

