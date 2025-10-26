CC ?= cc
CFLAGS ?= -O2
# Strict, portable C
CFLAGS += -std=c99 -Wall -Wextra -pedantic -Wno-unused-parameter
LDFLAGS ?=

# Try to discover libcmark via pkg-config when vendored sources are not present
CMARK_SRC_DIR := third_party/cmark/src
CMARK_HAVE_SRC := $(wildcard $(CMARK_SRC_DIR)/*.c)
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
  $(SRC_DIR)/math.c

INC_FLAGS := -I$(INC_DIR)

# If vendored cmark sources exist, compile them in; else try system libcmark.
ifeq (,$(CMARK_HAVE_SRC))
  ifneq (,$(CMARK_LIBS))
    CFLAGS += $(CMARK_CFLAGS) -DHAVE_LIBCMARK
    LDLIBS += $(CMARK_LIBS)
  else
    $(warning libcmark (cmark) not found; building without Markdown AST parsing. Rendering will be passthrough for now.)
  endif
else
  # Build with vendored cmark
  CMARK_SRC := $(CMARK_SRC_DIR)/cmark.c $(CMARK_SRC_DIR)/node.c $(CMARK_SRC_DIR)/block.c $(CMARK_SRC_DIR)/inlines.c $(CMARK_SRC_DIR)/iterator.c $(CMARK_SRC_DIR)/blocks.c $(CMARK_SRC_DIR)/scanners.c $(CMARK_SRC_DIR)/utf8.c $(CMARK_SRC_DIR)/buffer.c $(CMARK_SRC_DIR)/references.c $(CMARK_SRC_DIR)/man.c $(CMARK_SRC_DIR)/html.c $(CMARK_SRC_DIR)/latex.c $(CMARK_SRC_DIR)/xml.c $(CMARK_SRC_DIR)/commonmark.c
  SRC += $(CMARK_SRC)
  CFLAGS += -DHAVE_LIBCMARK -Ithird_party/cmark/src
endif

OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(SRC_DIR)/%.c,$(SRC))) \
       $(patsubst third_party/cmark/%.c,$(BUILD_DIR)/third_party/cmark/%.o,$(filter third_party/cmark/%.c,$(SRC)))

BIN := mdvic

.PHONY: all clean fmt

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

$(BUILD_DIR)/third_party/cmark/%.o: third_party/cmark/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ithird_party/cmark/src -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN)

fmt:
	@echo "No formatter configured; skip."

