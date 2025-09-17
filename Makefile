CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Werror -Iinclude -fPIC
LDFLAGS = -shared

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_M),x86_64)
    ARCH = x64
else ifeq ($(UNAME_M),aarch64)
    ARCH = arm64
else ifeq ($(UNAME_M),arm64)
    ARCH = arm64
else
    $(error Unsupported architecture: $(UNAME_M))
endif

ifeq ($(UNAME_S),Linux)
    BACKEND ?= both
    ifeq ($(BACKEND),x11)
        PLATFORM_SRC = src/linux_x11.c src/platform_linux.c
        PLATFORM_LIBS = -lX11
        CFLAGS += -DPODI_BACKEND_X11_ONLY
    else ifeq ($(BACKEND),wayland)
        PLATFORM_SRC = src/linux_wayland.c src/platform_linux.c
        PLATFORM_LIBS = -lwayland-client
        CFLAGS += -DPODI_BACKEND_WAYLAND_ONLY
    else
        PLATFORM_SRC = src/linux_x11.c src/linux_wayland.c src/platform_linux.c
        PLATFORM_LIBS = -lX11 -lwayland-client
        CFLAGS += -DPODI_BACKEND_BOTH
    endif
    LIB_EXT = .so
    CFLAGS += -DPODI_PLATFORM_LINUX
    ifeq ($(ARCH),x64)
        CFLAGS += -DPODI_ARCH_X64
    else ifeq ($(ARCH),arm64)
        CFLAGS += -DPODI_ARCH_ARM64
    endif
else ifeq ($(UNAME_S),Darwin)
    PLATFORM_SRC = src/macos_cocoa.m
    PLATFORM_LIBS = -framework Cocoa
    LIB_EXT = .dylib
    CFLAGS += -fobjc-arc -DPODI_PLATFORM_MACOS
    ifeq ($(ARCH),arm64)
        CFLAGS += -DPODI_ARCH_ARM64 -arch arm64
        LDFLAGS += -arch arm64
    else
        $(error macOS x64 not supported - only arm64 macOS is supported)
    endif
else
    $(error Unsupported platform: $(UNAME_S))
endif

SRCDIR = src
OBJDIR = obj
LIBDIR = lib
EXAMPLEDIR = examples
PROTOCOLDIR = protocols

SOURCES = $(SRCDIR)/podi.c $(PLATFORM_SRC)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJECTS := $(OBJECTS:$(SRCDIR)/%.m=$(OBJDIR)/%.o)

LIBRARY = $(LIBDIR)/libpodi$(LIB_EXT)

ifeq ($(UNAME_S),Linux)
ifneq ($(BACKEND),x11)
    PROTOCOL_HEADERS = $(SRCDIR)/xdg-shell-client-protocol.h
    PROTOCOL_SOURCES = $(SRCDIR)/xdg-shell-protocol.c
    SOURCES += $(PROTOCOL_SOURCES)
    OBJECTS += $(OBJDIR)/xdg-shell-protocol.o
endif
endif

.PHONY: all clean examples install protocols

all: $(LIBRARY)

ifeq ($(UNAME_S),Linux)
ifneq ($(BACKEND),x11)
$(SRCDIR)/xdg-shell-client-protocol.h:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $@

$(SRCDIR)/xdg-shell-protocol.c:
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $@

protocols: $(PROTOCOL_HEADERS) $(PROTOCOL_SOURCES)
endif
endif

ifeq ($(UNAME_S),Linux)
ifneq ($(BACKEND),x11)
$(LIBRARY): protocols $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(PLATFORM_LIBS)
else
$(LIBRARY): $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(PLATFORM_LIBS)
endif
else
$(LIBRARY): $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(PLATFORM_LIBS)
endif

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.m | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
	$(MAKE) -C $(EXAMPLEDIR) clean
ifeq ($(UNAME_S),Linux)
ifneq ($(BACKEND),x11)
	rm -f $(PROTOCOL_HEADERS) $(PROTOCOL_SOURCES)
endif
endif

examples: $(LIBRARY)
	$(MAKE) -C $(EXAMPLEDIR)

install: $(LIBRARY)
	sudo cp $(LIBRARY) /usr/local/lib/
	sudo cp include/podi.h /usr/local/include/
	sudo ldconfig 2>/dev/null || true

debug: CFLAGS += -g -DDEBUG
debug: $(LIBRARY)

release: CFLAGS += -O3 -DNDEBUG
release: $(LIBRARY)

x11: 
	$(MAKE) BACKEND=x11

wayland:
	$(MAKE) BACKEND=wayland

both:
	$(MAKE) BACKEND=both