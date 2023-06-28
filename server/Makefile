CC ?= $(CROSS_COMPILE)gcc
SRC = aesdsocket.c
TARGET ?= aesdsocket
OBJS = $(SRC:.c=.o)
LDFLAGS ?= -pthread -lpthread -lrt

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
