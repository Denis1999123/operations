CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -pedantic -O2
TARGET := sed_simplified
SOURCES := main.c operations.c
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

test: $(TARGET)
	@set -e; \
	TMP=./tmp_test.txt; \
	printf "one\ntwo\nthree\na/b\n" > $$TMP; \
	./$(TARGET) $$TMP 's/o/O/'; \
	./$(TARGET) $$TMP '/^tw/d'; \
	./$(TARGET) $$TMP 's/^/[PFX] /'; \
	./$(TARGET) $$TMP 's/$$/ [SFX]/'; \
	./$(TARGET) $$TMP 's/a\/b/SLASH/'; \
	echo "----- test file -----"; \
	cat $$TMP; \
	rm -f $$TMP
