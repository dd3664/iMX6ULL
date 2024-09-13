# Makefile

# 编译器设置
CC ?= gcc
CFLAGS := -fPIC -Wall -Werror -std=gnu11 -g -I./include -I../../../opensource/libunwind-1.5/install/include/
LDFLAGS := -shared -ldl -lpthread -L ../../../opensource/libunwind-1.5/install/lib -lunwind -lunwind-generic

# 源文件和目标设置
SRCDIR := src
OBJDIR := obj
BINDIR := bin
SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
EXECUTABLE := $(BINDIR)/libumem_tracer.so

# 默认目标
all: $(EXECUTABLE)

# 链接目标文件生成可执行文件
$(EXECUTABLE): $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

# 编译源文件生成目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

# 声明伪目标，避免和同名文件冲突
.PHONY: all clean
