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

HTTPDIR = examples/http
HTTPSRC =  $(wildcard $(HTTPDIR)/*.c)
HTTPOUTDIR = $(OUT_DIR)/http
HTTPOBJS = $(addprefix $(HTTPOUTDIR)/, $(notdir $(HTTPSRC:.c=.o)))
HTTPTARGET = $(OUT_DIR)/http$(EXEC)


http: all $(HTTPTARGET)

$(HTTPTARGET): $(HTTPOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBA) $(LDFLAGS)

$(HTTPOUTDIR)/%.o: $(HTTPDIR)/%.c | $(HTTPOUTDIR)
	$(CC) $(CFLAGS) -c -o $@ $< -I$(SRC_DIR)

$(HTTPOUTDIR):
	mkdir -p $(HTTPOUTDIR)

clean-http:
	rm -f $(HTTPTARGET) $(HTTPOUTDIR)/*.o

clean:
	rm -f $(OUT_DIR)/*.o $(TARGET) $(LIBA)
	make clean-http

http-rebuild: clean-http http
	echo "Rebuild http"

all-rebuild: clean all
	echo "Rebuild all"

.PHONY: all clean
