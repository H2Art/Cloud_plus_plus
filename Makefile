.RECIPEPREFIX := >
CC      ?= gcc
CFLAGS  ?= -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-format-truncation \
           -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD   ?= $(CURDIR)/build

# core: логика без HTTP. Каждый .c подхватывается автоматически.
CORE_SRC := $(wildcard src/core/*.c)
CORE_HDR := $(wildcard src/core/*.h)
CORE_OBJ := $(patsubst src/core/%.c,$(BUILD)/core/%.o,$(CORE_SRC))

# http: слой маршрутов БЕЗ Mongoose. Файлы mg_*.c (склейка с Mongoose)
# сюда не входят и собираются отдельно.
HTTP_SRC := $(filter-out src/http/mg_%.c,$(wildcard src/http/*.c))
HTTP_HDR := $(wildcard src/http/*.h)
HTTP_OBJ := $(patsubst src/http/%.c,$(BUILD)/http/%.o,$(HTTP_SRC))

# тесты: tests/<слой>/test_*.c -> отдельный бинарник на файл
TEST_SRC := $(wildcard tests/core/test_*.c) $(wildcard tests/http/test_*.c)
TESTS    := $(patsubst tests/%.c,$(BUILD)/tests/%,$(TEST_SRC))

.PHONY: test clean
test: $(TESTS)
> @for t in $(TESTS); do $$t || exit 1; done

$(BUILD)/core/%.o: src/core/%.c $(CORE_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD)/http/%.o: src/http/%.c $(CORE_HDR) $(HTTP_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD)/tests/%: tests/%.c $(CORE_OBJ) $(HTTP_OBJ) tests/test.h $(CORE_HDR) $(HTTP_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -Itests -o $@ $< $(CORE_OBJ) $(HTTP_OBJ)

clean:
> rm -rf $(BUILD)
