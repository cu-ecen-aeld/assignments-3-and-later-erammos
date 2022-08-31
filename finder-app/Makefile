ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu)
	CC := aarch64-none-linux-gnu-gcc
else
	CC := gcc
endif

writer: writer.o
	${CC} writer.o -o writer
writer.o:
	${CC} -c writer.c -o writer.o
clean:
	-rm -f writer.o writer

