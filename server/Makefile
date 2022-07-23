CROSS_COMPILE=

CFLAGS= -g -Wall -Werror

default: aesdsocket

all: aesdsocket

writer:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm aesdsocket
