CROSS_COMPILE=

CFLAGS= -g -Wall -Werror

default: aesdsocket

all: aesdsocket

aesdsocket:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm aesdsocket
