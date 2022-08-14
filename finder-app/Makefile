ifeq ($(CROSS_COMPILE), arm-none-linux-gnueabihf)
	CC := arm-none-linux-gnueabihf-gcc
else
	CC := gcc
endif

writer: writer.o
	${CC} writer.o -o writer
writer.o:
	${CC} -c writer.c -o writer.o
clean:
	-rm -f writer.o writer

