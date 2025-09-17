CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -I../include
LDFLAGS = -L../lib -lpodi -Wl,-rpath,../lib

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    PLATFORM_LIBS = -lX11
    LIB_EXT = .so
else ifeq ($(UNAME_S),Darwin)
    PLATFORM_LIBS = -framework Cocoa
    LIB_EXT = .dylib
    LDFLAGS = -L../lib -lpodi
endif

LDFLAGS += $(PLATFORM_LIBS)

.PHONY: all clean run run-x11 run-wayland

all: ../lib/libpodi$(LIB_EXT) demo

../lib/libpodi$(LIB_EXT):
	$(MAKE) -C ..

demo: demo.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

run: demo
	LD_LIBRARY_PATH=../lib ./demo

run-x11: demo
	PODI_BACKEND=x11 LD_LIBRARY_PATH=../lib ./demo

run-wayland: demo
	PODI_BACKEND=wayland LD_LIBRARY_PATH=../lib ./demo

clean:
	rm -f demo