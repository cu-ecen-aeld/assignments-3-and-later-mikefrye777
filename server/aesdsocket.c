/*
 * Mike Frye, July, 2022
 *
 */

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

bool term_received;

void flip_term_flag( int signum ) {
    if ( signum == SIGINT || signum == SIGTERM ) {
	term_received = true;	
    }
}

int main(int argc, char** argv)
{
    
    // init log
    openlog(NULL,0,LOG_USER);

    // initialize termination flag
    term_received = false;

    // register hander for SIGINT and SIGTERM
    struct sigaction term_receiver;
    memset(&term_receiver, 0, sizeof(struct sigaction));
    term_receiver.sa_handler = flip_term_flag;
    if ( sigaction(SIGTERM, &term_receiver, NULL) != 0 ) {
        syslog(LOG_ERR,"Failed registering action for SIGTERM, exiting : %s",strerror(errno));
	return -1;
    }
    
    if ( sigaction(SIGINT, &term_receiver, NULL) != 0 ) {
        syslog(LOG_ERR, "Failed registering action for SIGINT, exiting : %s",strerror(errno));
        return -1;
    }
    syslog(LOG_DEBUG, "Registered signal handlers");

    // check for daemon mode
    bool daemon_mode = false;
    if ( argc == 2 && ( strcmp(argv[1], "-d") == 0 ) ) {
        daemon_mode = true;
	syslog(LOG_DEBUG, "Running server as daemon");
    }
    else if ( argc > 1 ) {
        syslog(LOG_ERR, "Invalid input, use: aesdsocket [-d]");
    }

    // setup server
    int listener_sock = socket(PF_INET,SOCK_STREAM,0);
    if ( listener_sock == -1 ) {
        syslog(LOG_ERR, "Failed initializing socket, exiting : %s",strerror(errno));
	return -1;
    }
    // largely reused from https://beej.us/guide/bgnet/html/#getaddrinfoprepare-to-launch
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;  

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;    

    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo error, exiting : %s", gai_strerror(status));
	return -1;
    }

    if ( bind(listener_sock,servinfo->ai_addr,sizeof(struct sockaddr)) != 0 ) {
        syslog(LOG_ERR, "Failed socket bind to address, exiting : %s", strerror(errno));
	freeaddrinfo(servinfo);
	close(listener_sock);
	return -1;
    }

    // free server address info
    freeaddrinfo(servinfo);

    // bind successful, if daemon mode, fork here   
    pid_t fpid;
    int null_fd;

    if ( daemon_mode ) { fpid = fork(); }

    if ( daemon_mode ) {
	if ( fpid == -1 ) {
            syslog(LOG_ERR, "Fork failed, exiting : %s",strerror(errno));
	    close(listener_sock);
            return -1;
	}
	
	if ( fpid == 0 ) {
            // child  
	    // set up itself as a daemon
	    if ( setsid() == -1 ) {
                syslog(LOG_ERR, "Daemon failed starting own session : %s", strerror(errno));
		close(listener_sock);
		return -1;
	    }
	    if ( chdir("/") == -1 ) {
                syslog(LOG_ERR, "Daemon failed chdir : %s", strerror(errno));
                close(listener_sock);
                return -1;
            }
	    if ( ( null_fd = open("/dev/null", O_RDWR) ) == -1 ) {
                syslog(LOG_ERR, "Daemon failed opening /dev/null : %s", strerror(errno));
                close(listener_sock);
                return -1;
	    }
	    if ( dup2(null_fd,1) < 0 ) // for the rest of this process, stdout goes to null, not tty
	    {
                syslog(LOG_ERR, "Daemon redirecting stdout to /dev/null : %s", strerror(errno));
                close(listener_sock);
		close(null_fd);
                return -1;
	    }
	    if ( dup2(null_fd,2) < 0 ) // for the rest of this process, stderr goes to null, not tty
            {
                syslog(LOG_ERR, "Daemon failed redirecting stderr to /dev/null : %s", strerror(errno));
                close(listener_sock);
                close(null_fd);
                return -1;
            }
	    if ( dup2(null_fd,0) < 0 ) // for the rest of this process, stdin comes from null, not tty
            {
                syslog(LOG_ERR, "Daemon failed redirecting stdin to /dev/null : %s", strerror(errno));
                close(listener_sock);
                close(null_fd);
                return -1;
            }
	    // all std streams point to null, can close fd from open to null
	    close(null_fd);

	}
	else {
            // parent
	    // close fd on listener socket and exit successfully
	    close(listener_sock);
	    return 0;
	}

    }

    // at this point either the child has dropped through to opening the server, or this isn't a daemon

    if ( listen(listener_sock,2) != 0 ) {
        syslog(LOG_ERR, "Failed socket listen, exiting : %s", strerror(errno));
	close(listener_sock);
	return -1;
    }

    // client file handle
    FILE *cfh = fopen("/var/tmp/aesdsocketdata","a+");
    if ( cfh == NULL ) {
        syslog(LOG_ERR,"Failed opening data file : %s", strerror(errno));
        close(listener_sock);
        return -1;
    }

    // buffer init, start with 1 kB
    size_t bufinc = 1024; // buffer increase increment
    size_t bufsize = bufinc;
    size_t size_to_read = bufsize;
    void *alloc_base = malloc(bufsize*sizeof(char)); // start with 1 kB
    void *buf = alloc_base;
    void *inc_start = buf; // increment start, use char*?
    memset(buf, 0, bufsize*sizeof(char));
    ssize_t numread;
    ssize_t numwritten;
    bool dropped = false;


    while ( true ) {
        
	struct sockaddr_in client_address;
        memset(&client_address,0,sizeof(client_address));
	size_t claddrsz = sizeof(client_address);
        int accepted_sock = accept(listener_sock, (struct sockaddr *)&client_address, (socklen_t *)&claddrsz);
        if ( accepted_sock == -1 ) {
            if ( errno == EINTR ) {
                syslog(LOG_INFO, "Caught signal, exiting");
                free(alloc_base);
                if ( fclose(cfh) != 0 ) {
                    syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno));
                }
                if ( close(listener_sock) == -1 ) {
                    syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
                }
                if ( remove("/var/tmp/aesdsocketdata") == -1 ) {
                    syslog(LOG_ERR, "Failed removing client file, %s",strerror(errno));
                }

		return 0;

	    }
	    else {
                syslog(LOG_ERR, "Failed connection accept, exiting : %s", strerror(errno));
		free(alloc_base);
                if ( fclose(cfh) != 0 ) {
                    syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno));
                }
                if ( close(listener_sock) == -1 ) {
                    syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
                }
		// leave temp file as an artifact of unclean exit
                return -1;
	    }
        }

        // connection established, need to log, writing address in capital hex
        syslog(LOG_DEBUG,"Accepted connection from %X",client_address.sin_addr.s_addr);


        // 0 return means connection closed, -1 return means error
        while ( ( numread = recv(accepted_sock, inc_start, size_to_read, 0) ) > 0 ) {
            // check the last byte of this increment
            char *last = (char*)inc_start + numread - 1;
            if ( *last == '\n' ) {
	        // packet is complete
		size_t packet_length = bufsize - size_to_read + numread;
		syslog(LOG_DEBUG, "Received packet of size %ld",packet_length);
	    
	        // catch corner case where we perfectly filled the buffer increment
	        if ( numread == size_to_read ) {
		    syslog(LOG_INFO, "Wrote packet exactly to boundary");
                    alloc_base = realloc(buf,(bufsize+1)*sizeof(char));
                    if (alloc_base == NULL) {
                        syslog(LOG_ERR,"Buffer realloc failed! Requested %ld. Dropping packet.", bufsize+1);
                        // reset buffer, unnecessary, we will fall through to the end of the packet end case
                        dropped = true;
                    }
                    else {
		        buf = alloc_base;
			bufsize+=1;
		        // add null terminator
		        *( (char*)buf + bufsize ) = '\0';
		    }
		
	        }
	        if ( !dropped ) {
		    size_t left_to_write = packet_length;
		    char *writestart = (char*)buf;
	            // write new string to temp file	   
		    
		    do {
		        numwritten = fprintf(cfh, "%s", writestart); 
			if ( numwritten < 0 ) {
                            syslog(LOG_ERR,"Failure writing to client file : %s", strerror(errno));
                        }
			else {
			    left_to_write -= numwritten;
			    if ( left_to_write > 0 ) {
                                syslog(LOG_DEBUG,"Only wrote %ld bytes to file, %ld still left to write", numwritten, left_to_write);
				writestart += numwritten;
			    }
			}
		    } while ( left_to_write > 0 && numwritten > 0 );
	        }
                // write back all file contents (echo full packet history) one packet at a time
                // seek to file start
                rewind(cfh);
                // print line by line, recycle socket buffer
                while ((numread = getline((char**)&buf, &bufsize, cfh)) != -1) {
                    if ( ( numwritten = send(accepted_sock, buf, numread, 0) ) < numread ) {
                        syslog(LOG_ERR,"Wrote fewer bytes (%ld) to socket than were read from file (%ld) for this line", numwritten, numread);
                    }
	        }
	    
	        // seek to file end for next write
	        if ( fseek(cfh, 0, SEEK_END) == -1 ) {
                    syslog(LOG_ERR, "Failed seek after returning temp file to client : %s", strerror(errno));
	        }

                // clear buffer
	        inc_start = buf;
		size_to_read = bufsize;
                memset(buf, 0, bufsize*sizeof(char));

	        // reset dropped flag
	        dropped = false;
            }
	    else { // realloc buf and put inc start at the right location to read next kB input of the current packet
		//syslog(LOG_DEBUG, "Current buffer of size %ld to be realloc'ed to size %ld",bufsize, bufsize + bufinc);
                alloc_base = realloc(buf,(bufsize + bufinc)*sizeof(char));
                if (alloc_base == NULL) {
                    syslog(LOG_ERR,"Buffer realloc failed! Requested %ld. Dropping packet.", bufsize+1);
		    dropped = true;
	        }
	        else {
	            buf = alloc_base;
	            inc_start = (void *)( (char*)buf + bufsize );
		    bufsize += bufinc;
		    size_to_read = bufinc;
	        }
	    }

        }

        // check if connection close or error
        if ( numread == -1 ) {
            syslog(LOG_ERR, "revc returned failure code, exiting : %s", strerror(errno));
	    free(alloc_base);
            if ( fclose(cfh) != 0 ) {
                syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno));
            }
            if ( close(listener_sock) == -1 ) {
                syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
            }
            if ( close(accepted_sock) == -1 ) {
                syslog(LOG_ERR, "Failed closing connection socket, %s",strerror(errno));
            }
	    // leave temp file as artifact of unclean exit
            return -1;
	}
	// else, close accepted_sock to accept a new one
	close(accepted_sock);

	// cleanly exit by closing the listener socket, client file, and freeing buffer
	// also delete client file and then exit the program
	if ( term_received ) {
            syslog(LOG_INFO, "Caught signal, exiting");
            free(alloc_base);
	    if ( fclose(cfh) != 0 ) {
	        syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno)); 	    
	    }
	    if ( close(listener_sock) == -1 ) {
                syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
	    }
	    if ( remove("/var/tmp/aesdsocketdata") == -1 ) {
                syslog(LOG_ERR, "Failed removing client file, %s",strerror(errno));		
            }

	    return 0;
	}
    
    }

    //return 0;
}

