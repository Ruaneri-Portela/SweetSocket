CC = gcc
CFLAGS = -Wall -Wextra -pedantic -g
LDFLAGS =

TARGET = SweetSocket
LIBNAME := $(TARGET)

out_dir = build
src_dir = src
src_files = $(wildcard $(src_dir)/*.c)
src_files += $(wildcard $(sweetthread_dir)/*.c)

sweetthread_dir = $(src_dir)/SweetThread

obj = $(addprefix $(out_dir)/, $(notdir $(src_files:.c=.o)))
lib := $(TARGET).a

TARGET := $(out_dir)/$(TARGET)

ifeq ($(OS),Windows_NT)
	LDFLAGS += -lws2_32
	TARGET := $(TARGET).dll
else ifeq ($(shell uname),Linux)
	LDFLAGS += -lpthread
	TARGET := $(TARGET).so
endif

all: $(TARGET) $(lib)

$(out_dir):
	mkdir -p $(out_dir)

$(out_dir)/%.o: $(src_dir)/%.c | $(out_dir)
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(out_dir)/%.o: $(sweetthread_dir)/%.c | $(out_dir)
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(TARGET): $(obj) 
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(lib): $(objs)
	ar rcs $@ $^

clean:
	rm -rf $(out_dir)/*.o $(TARGET) $(lib)

.PHONY: all clean