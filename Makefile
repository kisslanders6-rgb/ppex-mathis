CC = gcc
CFLAGS = -std=c99 -pedantic -Werror -Wall -Wextra -Wvla

SRC = src/command_line.c src/execution.c src/fifo.c src/parser.c \
      src/rules.c src/safe_memory.c src/text_tools.c src/variables.c
OBJ = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
HEADERS = $(wildcard src/*.h)

UNIT_BIN = tests/unit_tests
UNIT_OBJ = tests/unit_tests.o
FUNC_BIN = tests/functional_tests
FUNC_OBJ = tests/functional_tests.o tests/functional_test_helpers.o
TEST_OBJ = $(UNIT_OBJ) $(FUNC_OBJ)

# the macros of criterion are not accepted by -std=c99 -pedantic
$(TEST_OBJ): CFLAGS = -std=gnu99 -Wall -Wextra -Werror
$(TEST_OBJ): CPPFLAGS = -Isrc

TEST_LDLIBS = -lcriterion
ifeq ($(shell uname), Darwin)
$(TEST_OBJ): CPPFLAGS += -I/opt/homebrew/include
TEST_LDLIBS += -L/opt/homebrew/lib
endif

all: minimake

minimake: $(MAIN_OBJ) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(MAIN_OBJ) $(OBJ)

$(MAIN_OBJ) $(OBJ) $(TEST_OBJ): $(HEADERS)

$(UNIT_BIN): $(UNIT_OBJ) $(OBJ)
	$(CC) -o $@ $(UNIT_OBJ) $(OBJ) $(TEST_LDLIBS)

$(FUNC_BIN): $(FUNC_OBJ)
	$(CC) -o $@ $(FUNC_OBJ) $(TEST_LDLIBS)

check: minimake $(UNIT_BIN) $(FUNC_BIN)
	./$(UNIT_BIN)
	./$(FUNC_BIN)

clean:
	$(RM) minimake $(MAIN_OBJ) $(OBJ) $(TEST_OBJ) $(UNIT_BIN) $(FUNC_BIN)

.PHONY: all check clean
