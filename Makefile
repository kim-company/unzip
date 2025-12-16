# Makefile for Deflate64 NIF

# Detect OS
UNAME_S := $(shell uname -s)

# Output directory
PRIV_DIR = $(MIX_APP_PATH)/priv
NIF_SO = $(PRIV_DIR)/deflate64_nif.so

# Source files
C_SRC_DIR = c_src
SOURCES = $(C_SRC_DIR)/deflate64_nif.c \
          $(C_SRC_DIR)/infback9.c \
          $(C_SRC_DIR)/inftree9.c

# Erlang include path
ERTS_INCLUDE_DIR ?= $(shell erl -noshell -eval "io:format(\"~ts/erts-~ts/include/\", [code:root_dir(), erlang:system_info(version)])." -s erlang halt)

# Compiler flags
CFLAGS = -O3 -std=c99 -finline-functions -Wall -Wmissing-prototypes
CFLAGS += -fPIC -I$(ERTS_INCLUDE_DIR) -I$(C_SRC_DIR)

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
    # macOS
    LDFLAGS = -dynamiclib -undefined dynamic_lookup
    CFLAGS += -arch arm64 -arch x86_64
else ifeq ($(UNAME_S),Linux)
    # Linux
    LDFLAGS = -shared
    CFLAGS += -fPIC
else
    # Assume other Unix-like
    LDFLAGS = -shared
endif

# Link with zlib
LDFLAGS += -lz

# Default target
all: $(NIF_SO)

$(PRIV_DIR):
	mkdir -p $(PRIV_DIR)

$(NIF_SO): $(PRIV_DIR) $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(NIF_SO)

clean:
	rm -f $(NIF_SO)

.PHONY: all clean
