/**
 *  @filename: writer.c
 *  @brief   : This program expects 2 arguments: the path of file
 * 	       and the string to overwrite. The program opens the
 *	       given file and overwrites the existng data with the
 * 	       string provided in the arguments.
 *	       If the file does not exist, a file with the same
 * 	       given name is created and the write operation is performed.
 *
 * @arg      : char* file_path
 *
 * @arg	     : char* string_to_be_written
 *
 *
 * @author   : Sharath Jonnala
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

/*
 * @brief : This method gives information about the
 *	    arguments to the user.
 *	    It is called only when there is a mismatch
 *	    in expected arguments.
 *
 * @return: int
 */
int information_display() {

   printf("Arguments mimatch. Enter arguments as follows:\n");
   printf("Arg1: Path of file\n");
   printf("Arg2: String to insert\n");
   return 1;
}


/*
 * @brief: Entry point of the program, contains all the business logic
 */
int main(int argc, char **argv) {

   openlog("finder-app-writer.c", LOG_PID, LOG_USER);

   int total_args_passed = argc - 1;	// Subtracting one to discard execution file name parameter

   // Checking for invalid arguments
   if(total_args_passed > 2 || total_args_passed < 2)
   {
	syslog(LOG_ERR, "Invalid Arguments. Received %d arguments instead of 2", argc);
	information_display();
	exit(1);
   }


   char* file_path = argv[1];
   char* wr_string = argv[2];

   // File Open and Write
   int fd;

   fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);

   if (fd == -1)
   {
	syslog(LOG_ERR, "Error in opening file");
   } else {
		int buf_count = 0;
		int wr;
		for(buf_count = 0 ; wr_string[buf_count] != '\0' ; buf_count++);
		syslog(LOG_DEBUG, "Writing %s to %s", wr_string, file_path);

		wr = write(fd, wr_string, buf_count);

		if(wr == -1)
		{
			syslog(LOG_ERR, "Error in writing to file");
		}
	}
   if (close (fd) == -1)
   {
	syslog(LOG_ERR, "Error in closing file");
   }

   closelog();
}
