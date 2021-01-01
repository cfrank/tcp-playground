TARGET = server.out
LIBS =
CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200112L -g -O0 -Wall -Werror
OUTPUT = bin/

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = server.o
HEADERS = server.h

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS) | $(OUTPUT)
	$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) -o $(OUTPUT)$@

$(OUTPUT):
	@echo "Creating $(OUTPUT)"
	@mkdir -p $@

clean:
	@echo "Cleaning up"
	-rm -f $(SRC)*.o
	-rm -rf $(OUTPUT)
