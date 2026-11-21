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
> @if command -v node >/dev/null 2>&1; then node tests/js/test_app.js || exit 1; \
>  else echo "node not found: JS tests skipped"; fi

$(BUILD)/core/%.o: src/core/%.c $(CORE_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD)/http/%.o: src/http/%.c $(CORE_HDR) $(HTTP_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD)/tests/%: tests/%.c $(CORE_OBJ) $(HTTP_OBJ) tests/test.h $(CORE_HDR) $(HTTP_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -Isrc -Itests -o $@ $< $(CORE_OBJ) $(HTTP_OBJ)

# ---- боевая сборка: make build  ->  build/release/cloud ----
# Требует src/vendor/mongoose.c и mongoose.h. Без санитайзеров.
# Статический бинарник для сервера: make build LDFLAGS=-static
REL        := $(BUILD)/release
REL_CFLAGS := -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-format-truncation \
              -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong
APP_CFLAGS := -std=gnu11 -D_GNU_SOURCE -Wall -Wextra -O2 -D_FORTIFY_SOURCE=2 \
              -fstack-protector-strong
GLUE_SRC   := $(wildcard src/http/mg_*.c) src/main.c
R_CORE_OBJ := $(patsubst src/core/%.c,$(REL)/core/%.o,$(CORE_SRC))
R_HTTP_OBJ := $(patsubst src/http/%.c,$(REL)/http/%.o,$(HTTP_SRC))
R_GLUE_OBJ := $(patsubst src/%.c,$(REL)/%.o,$(GLUE_SRC))
R_VENDOR   := $(REL)/vendor/mongoose.o

.PHONY: build
build: $(REL)/cloud

$(R_CORE_OBJ): $(REL)/core/%.o: src/core/%.c $(CORE_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(REL_CFLAGS) -Isrc -c -o $@ $<

$(R_HTTP_OBJ): $(REL)/http/%.o: src/http/%.c $(CORE_HDR) $(HTTP_HDR)
> @mkdir -p $(dir $@)
> $(CC) $(REL_CFLAGS) -Isrc -c -o $@ $<

$(R_GLUE_OBJ): $(REL)/%.o: src/%.c $(CORE_HDR) $(HTTP_HDR) src/vendor/mongoose.h
> @mkdir -p $(dir $@)
> $(CC) $(APP_CFLAGS) -Isrc -Isrc/vendor -c -o $@ $<

$(R_VENDOR): src/vendor/mongoose.c src/vendor/mongoose.h
> @mkdir -p $(dir $@)
> $(CC) -std=gnu11 -D_GNU_SOURCE -O2 -w -c -o $@ $<

$(REL)/cloud: $(R_CORE_OBJ) $(R_HTTP_OBJ) $(R_GLUE_OBJ) $(R_VENDOR)
> $(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
> rm -rf $(BUILD)
