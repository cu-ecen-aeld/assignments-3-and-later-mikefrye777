//	Mike Frye, July 2022, for Coursera course "Linux System Programming and Introduction to Buildroot"
//
//

#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


int main(int argc, char** argv)
{
	openlog(NULL,0,LOG_USER);
	if ( argc != 3 )
	{
		syslog(LOG_ERR,"Improper number of arguments. Need 2, received %d",argc-1);
		fprintf(stderr,"Improper number of arguments. Need 2, received %d\n",argc-1);
		return 1;
	}
	FILE *fd = fopen(argv[1],"w");
	if (!fd)
	{
		int errtmp = errno;
		syslog(LOG_ERR,"Failed to open file. %m");
		fprintf(stderr,"Failed to open file. %s\n",strerror(errtmp));
		return 1;
	}

	syslog(LOG_DEBUG,"Writing %s to %s", argv[2], argv[1]);

	if ( fprintf(fd,"%s",argv[2]) < 0 )
	{
		syslog(LOG_ERR,"Failed to write to file, but file was opened.");
                fprintf(stderr,"Failed to write to file, but file was opened.\n");
		if ( fclose(fd) ) //fclose returns 0 on success
		{
			int errtmp = errno;
			syslog(LOG_ERR,"Failed to close file. %m");
                	fprintf(stderr,"Failed to close file. %s\n", strerror(errtmp));
		}
		return 1;
	}
	
	if ( fclose(fd) ) //fclose returns 0 on success
        {
		int errtmp = errno;
		syslog(LOG_ERR,"Failed to close file. %m");
		fprintf(stderr,"Failed to close file. %s\n", strerror(errtmp));
		return 1;
	}

	return 0;
}
