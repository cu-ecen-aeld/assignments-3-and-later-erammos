CC ?= $(CROSS_COMPILE)gcc
SRC = aesdsocket.c
TARGET ?= aesdsocket
OBJS = $(SRC:.c=.o)
LDFLAGS ?= -pthread -lpthread -lrt
ifdef USE_AESD_CHAR_DEVICE
	CFLAGS += -DUSE_AESD_CHAR_DEVICE
endif
all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
