# Name of the output executable
TARGET = example

# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2

# Include path for headers (add the path to your header directory)
INCLUDES = -I../src

# Source files
SRCS = main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default rule to build the executable
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS)

# Rule to compile C source files into object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up intermediate files and the executable
clean:
	rm -f $(OBJS) $(TARGET)

# Phony target to prevent conflicts with files of the same name
.PHONY: all clean
