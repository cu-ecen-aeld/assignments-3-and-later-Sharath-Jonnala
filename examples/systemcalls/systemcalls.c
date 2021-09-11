#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/

    int sys =  system(cmd);

    if(sys == -1)
	return false;

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
   // char * arg_list[count];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
	//printf("command[%d]=%s\n",i,command[i]);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
   int wait_status;
    pid_t pid;
    int exec_ret =0;
    pid = fork();
    //printf("PID is %d  \n", pid);
    if(pid == -1) {
	return false;
    } else if(pid == 0) {
	//printf("ENTERIG IN PID 0\n");
    	execv(command[0], command);
	//printf("EXEC RETURN IS %d \n", exec_ret)
  	exit(-1);
	//return false;
    }
    else
    {

        int wait = waitpid(pid, &wait_status, 0);
        if(wait == -1) {
	    //printf("EXITING AT WAIT\n");
	    return false;
    	}

    	if(WIFEXITED(wait_status)) {
	    int exit_st = WEXITSTATUS(wait_status);
	    if(exit_st != 0) return false;
	//printf("EXITING AT WAIT_STATUS\n");
        }
    }
    va_end(args);

    if(exec_ret == -1)
    {
	//printf("EXITING AT EXEC_RET\n");
    	return false;
    }
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
  //  char * arg_list[count];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
/*
for(int j = 0 ; j < count - 1; j++)
    {
        arg_list[j-1] = command[j];
    }
    arg_list[count -1] = NULL;
*/
  command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    int kpid;
    int wait_status;
    //int fd = open( outputfile, O_WRONLY|O_TRUNC|O_CREAT, S_IRWXU);
    //printf("FD IS %d \n", fd);
   
/* if (fd < 0) {
	perror("open");
	return false;
    }*/
    kpid = fork();
    if(kpid == -1) return false;
    if(kpid == 0){
	int fd = open( outputfile, O_WRONLY|O_TRUNC|O_CREAT, S_IRWXU);
	if (dup2(fd, 1) < 0) {
	   perror("dup2");
	   return false;
	}
    	close(fd);
    	execv(command[0], command); perror("execv");
	//return false;
	exit(-1);
   } 
else {
        int wait = waitpid(kpid, &wait_status, 0);
        if(wait == -1) {
            //printf("EXITING AT WAIT\n");
            return false;
        }

   if(WIFEXITED(wait_status)) {
        int exit_st = WEXITSTATUS(wait_status);
        if(exit_st != 0) return false;
        //printf("EXITING AT WAIT_STATUS\n");
   }
}
    va_end(args);
    return true;
}
