CC = gcc
CFLAGS = -g
LDFLAGS =

TARGET = SweetSocket
LIBNAME := $(TARGET)

OUT_DIR = out
SRC_DIR = src
SWEETTHREAD_DIR = $(SRC_DIR)/SweetThread

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
SRC_FILES += $(wildcard $(SWEETTHREAD_DIR)/*.c)

OBJS = $(addprefix $(OUT_DIR)/, $(notdir $(SRC_FILES:.c=.o)))

TARGET := $(OUT_DIR)/$(TARGET)
LIBA := $(TARGET).a
EXEC =

ifeq ($(OS),Windows_NT)
	LDFLAGS += -lws2_32
	TARGET := $(TARGET).dll
	EXEC := .exe
else
endif

all: $(TARGET) libExport

$(TARGET): $(OBJS)
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o: $(SWEETTHREAD_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

libExport: $(LIBA)
$(LIBA): $(OBJS)
	ar rcs $@ $^

HTTPTARGET = $(OUT_DIR)/http$(EXEC)
HTTPCODE = examples/http/http.c
http: all $(HTTPTARGET)

$(HTTPTARGET): $(HTTPCODE) $(LIBA)
	$(CC) $(CFLAGS) -o $@ $< -L$(OUT_DIR) -l$(LIBNAME) -I$(SRC_DIR)

clean:
	rm -f $(OUT_DIR)/*.o $(TARGET) $(LIBA) $(HTTPTARGET)

.PHONY: all clean
