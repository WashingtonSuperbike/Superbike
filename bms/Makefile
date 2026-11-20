
# Compiler
CC = clang

# Compiler options
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude

# Source files needed for our balancing test
SOURCES = src/bms_config.c \
          src/bms_balancing.c \
		  src/bms_faults.c \
          tests/test_balancing.c

# Header files
HEADERS = $(wildcard include/*.h)

# Output executable
TEST = build/test_balancing

.PHONY: test clean

# Compile the test program
$(TEST): $(SOURCES) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(SOURCES) -o $(TEST)

# Run the test
test: $(TEST)
	./$(TEST)

# Remove compiled files
clean:
	rm -rf build
