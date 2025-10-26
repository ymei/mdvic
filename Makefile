CC ?= cc
CFLAGS ?= -O2
# Strict, portable C
CFLAGS += -std=c99 -Wall -Wextra -pedantic -Wno-unused-parameter
LDFLAGS ?=

# libcmark linkage: prefer locally built submodule library, else pkg-config
CMARK_SUBMODULE_DIR := third_party/cmark
CMARK_LOCAL_BUILD := $(CMARK_SUBMODULE_DIR)/build
CMARK_LOCAL_INC := -I$(CMARK_SUBMODULE_DIR)/src -I$(CMARK_LOCAL_BUILD)/src
CMARK_LIB_A := $(CMARK_LOCAL_BUILD)/src/libcmark.a
CMARK_LIB_SO := $(CMARK_LOCAL_BUILD)/src/libcmark.so
CMARK_LIB_DYLIB := $(CMARK_LOCAL_BUILD)/src/libcmark.dylib
CMARK_CFLAGS := $(shell pkg-config --cflags libcmark 2>/dev/null)
CMARK_LIBS   := $(shell pkg-config --libs   libcmark 2>/dev/null)

BUILD_DIR := build
SRC_DIR := src
INC_DIR := include

SRC := \
  $(SRC_DIR)/main.c \
  $(SRC_DIR)/renderer.c \
  $(SRC_DIR)/wrap.c \
  $(SRC_DIR)/wcwidth.c \
  $(SRC_DIR)/math.c \
  $(SRC_DIR)/lint.c

INC_FLAGS := -I$(INC_DIR)

# Link to libcmark if available
ifneq (,$(wildcard $(CMARK_LIB_A)))
  CFLAGS += -DHAVE_LIBCMARK $(CMARK_LOCAL_INC)
  LDFLAGS +=
  LDLIBS += $(CMARK_LIB_A)
else ifneq (,$(wildcard $(CMARK_LIB_SO)))
  CFLAGS += -DHAVE_LIBCMARK $(CMARK_LOCAL_INC)
  LDLIBS += $(CMARK_LIB_SO)
else ifneq (,$(wildcard $(CMARK_LIB_DYLIB)))
  CFLAGS += -DHAVE_LIBCMARK $(CMARK_LOCAL_INC)
  LDLIBS += $(CMARK_LIB_DYLIB)
else ifneq (,$(CMARK_LIBS))
  CFLAGS += -DHAVE_LIBCMARK $(CMARK_CFLAGS)
  LDLIBS += $(CMARK_LIBS)
else
  $(warning libcmark (cmark) not found; building without Markdown AST parsing. Rendering will be passthrough for now.)
endif

OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

BIN := mdvic

.PHONY: all clean fmt cmark test check

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN)

fmt:
	@echo "No formatter configured; skip."

# Build the cmark submodule locally with CMake (Release static library)
cmark:
	@if command -v cmake >/dev/null 2>&1; then \
	  echo "Configuring cmark in $(CMARK_LOCAL_BUILD)..."; \
	  cmake -S $(CMARK_SUBMODULE_DIR) -B $(CMARK_LOCAL_BUILD) -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF >/dev/null; \
	  echo "Building cmark..."; \
	  cmake --build $(CMARK_LOCAL_BUILD) --config Release -- -j >/dev/null; \
	  echo "cmark built."; \
	else \
	  echo "cmake not found; install CMake to build libcmark locally."; \
	  exit 1; \
	fi

test: all
	@echo "Running tests..."
	@WIDTH=40 MDVIC_NO_COLOR=1 MDVIC_NO_OSC8=1 tests/run.sh

check: test

.PHONY: wcwidth-table
wcwidth-table:
	@ENV_OK=1; \
	if [ -z "$$UNICODE_DIR" ]; then echo "Set UNICODE_DIR to directory containing UnicodeData.txt and EastAsianWidth.txt"; exit 1; fi; \
	sh tools/gen_wcwidth.sh
