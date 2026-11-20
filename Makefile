.RECIPEPREFIX := >
CC      ?= gcc
CFLAGS  ?= -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-format-truncation \
           -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD   ?= $(CURDIR)/build

# core: логика без HTTP. Каждый .c подхватывается автоматически.
CORE_SRC := $(wildcard src/core/*.c)
CORE_HDR := $(wildcard src/core/*.h)
CORE_OBJ := $(patsubst src/core/%.c,$(BUILD)/core/%.o,$(CORE_SRC))

# тесты: tests/core/test_*.c -> отдельный бинарник на файл
TEST_SRC := $(wildcard tests/core/test_*.c)
TESTS    := $(patsubst tests/core/%.c,$(BUILD)/tests/%,$(TEST_SRC))

.PHONY: test clean
test: $(TESTS)
> @for t in $(TESTS); do $$t || exit 1; done

$(BUILD)/core/%.o: src/core/%.c $(CORE_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD)/tests/%: tests/core/%.c $(CORE_OBJ) tests/test.h $(CORE_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -Itests -o $@ $< $(CORE_OBJ)

clean:
> rm -rf $(BUILD)