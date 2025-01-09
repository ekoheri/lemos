CC = gcc
CFLAGS = -Wall -Iinclude
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.0 libxml-2.0) -lcurl -lm

# Direktori
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

# File sumber dan objek
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BIN_DIR)/lemos
CONFIG_FILE  = config/lemos

# Flags tambahan dari pkg-config
PKG_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.0 libxml-2.0)
CFLAGS += $(PKG_CFLAGS)

# Aturan utama
all: $(TARGET)

# Membuat executable
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Membuat file objek
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	@cp $(CONFIG_FILE) $(BIN_DIR)

# Membersihkan file hasil kompilasi
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Membersihkan lebih lengkap (opsional)
dist-clean: clean
	rm -rf $(TARGET)
