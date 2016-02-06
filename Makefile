LIBS = libtoxcore libsodium
CFLAGS = $(shell pkg-config --cflags $(LIBS)) -std=gnu99 -Wall -Werror -ggdb -D_XOPEN_SOURCE_EXTENDED -D_XOPEN_SOURCE -D_FILE_OFFSET_BITS=64
OBJ = toxcs.o misc.o commands.o cs.o
LDFLAGS = $(shell pkg-config --libs $(LIBS))
SRC_DIR = ./src

all: $(OBJ)
	@echo "  LD    $@"
	@$(CC) $(CFLAGS) -o toxcs $(OBJ) $(LDFLAGS)

%.o: $(SRC_DIR)/%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) -o $*.o -c $(SRC_DIR)/$*.c
	@$(CC) -MM $(CFLAGS) $(SRC_DIR)/$*.c > $*.d

clean:
	rm -f *.d *.o toxcs

.PHONY: clean all
