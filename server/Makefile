ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu)
	CC := aarch64-none-linux-gnu-gcc
else
	CC := gcc
endif

aesdsocket: aesdsocket.o
	${CC} aesdsocket.o -o aesdsocket
aesdsocket.o:
	${CC}  -g -c -Wall -Werror aesdsocket.c -o aesdsocket.o -lpthread
clean:
	-rm -f aesdsocket.o aesdsocket

