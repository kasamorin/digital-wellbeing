.PHONY: all clean install uninstall

CC       := gcc
CFLAGS   := -std=gnu23 -Wall -Wextra -Wpedantic
LDFLAGS  :=

# Detect session type for idle backend selection at compile time (both available)
WAYLAND_SCANNER := $(shell which wayland-scanner 2>/dev/null)
WAYLAND_PROTOCOLS_DIR := $(shell pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)

# pkg-config dependencies
PKGS     := json-c dbus-1 xscrnsaver
PKGS_WL  := wayland-client

CFLAGS   += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  += $(shell pkg-config --libs $(PKGS))

# Wayland is optional at build time (runtime detection handles it)
ifneq ($(WAYLAND_PROTOCOLS_DIR),)
  CFLAGS += $(shell pkg-config --cflags $(PKGS_WL))
  LDFLAGS += $(shell pkg-config --libs $(PKGS_WL))
  CFLAGS += -DHAVE_WAYLAND
endif

SRCDIR   := src
BUILDDIR := build
PROTODIR := protocols

SOURCES  := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

# Wayland protocol codegen
WL_PROTO_XML := $(PROTODIR)/ext-idle-notify-v1.xml
WL_PROTO_C   := $(BUILDDIR)/ext-idle-notify-v1.c
WL_PROTO_H   := $(BUILDDIR)/ext-idle-notify-v1.h

ifneq ($(WAYLAND_SCANNER),)
  WL_CODEGEN := $(WL_PROTO_C) $(WL_PROTO_H)
endif

TARGET   := digital-wellbeing

all: dirs $(WL_CODEGEN) $(TARGET)

dirs:
	@mkdir -p $(BUILDDIR)

# Wayland protocol code generation
$(WL_PROTO_C) $(WL_PROTO_H): $(WL_PROTO_XML)
	$(WAYLAND_SCANNER) client-header < $< > $(WL_PROTO_H)
	$(WAYLAND_SCANNER) public-code < $< > $(WL_PROTO_C)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(WL_CODEGEN)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	install -Dm644 systemd/digital-wellbeing.service $(DESTDIR)/usr/lib/systemd/user/digital-wellbeing.service

uninstall:
	rm -f $(DESTDIR)/usr/bin/$(TARGET)
	rm -f $(DESTDIR)/usr/lib/systemd/user/digital-wellbeing.service