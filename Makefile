.PHONY: all clean install uninstall dirs pot po mo locales

CC       := gcc
CXX      := g++

# pkg-config dependencies
C_PKGS   := json-c
CXX_PKGS := Qt6Widgets Qt6Gui Qt6Core

# C standard (overridable for older toolchains, e.g. gnu2x on GCC 13)
CSTD     ?= gnu23

# C flags: gcc, C23
CFLAGS   := -std=$(CSTD) -Wall -Wextra -Wpedantic \
            $(shell pkg-config --cflags $(C_PKGS))

# C++ flags: g++, C++23, -fpic needed for gcc 16 + Qt6 protected symbols
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -fpic \
            $(shell pkg-config --cflags $(CXX_PKGS))

# Link flags — g++ does final link, pulls in libstdc++ + Qt6
LDFLAGS  := $(shell pkg-config --libs $(C_PKGS) $(CXX_PKGS))

SRCDIR   := src
BUILDDIR := build

# C sources (compiled with gcc)
C_SOURCES := $(wildcard $(SRCDIR)/*.c)
C_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(C_SOURCES))

# C++ sources (compiled with g++)
CXX_SOURCES := $(wildcard $(SRCDIR)/*.cpp)
CXX_OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CXX_SOURCES))

OBJECTS := $(C_OBJECTS) $(CXX_OBJECTS)

TARGET := digital-wellbeing

# Install paths. PREFIX defaults to ~/.local; DESTDIR is for staged installs
# (packaging). Override both, e.g. make install PREFIX=/usr DESTDIR="$pkgdir".
PREFIX  ?= $(HOME)/.local
LOCALEDIR ?= $(PREFIX)/share/locale

# i18n — gettext
PODIR     := po
LOCALEDIR_BUILD := $(BUILDDIR)/locale
LINGUAS   := $(shell cat $(PODIR)/LINGUAS 2>/dev/null)

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILDDIR)

# C compile rule
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# C++ compile rule
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link with g++ (pulls in libstdc++ + Qt6)
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)
	rm -f $(PODIR)/*.mo

# Extract translatable strings into POT template
pot:
	xgettext --keyword=_ --keyword=N_ --from-code=UTF-8 \
		--output=$(PODIR)/digital-wellbeing.pot \
		--package-name=digital-wellbeing \
		src/*.c src/*.cpp

# Merge POT into all PO files
po: pot
	@for lang in $(LINGUAS); do \
		if [ -f $(PODIR)/$$lang.po ]; then \
			msgmerge --update $(PODIR)/$$lang.po \
				$(PODIR)/digital-wellbeing.pot; \
		else \
			msginit --locale=$$lang \
				--input=$(PODIR)/digital-wellbeing.pot \
				--output=$(PODIR)/$$lang.po; \
		fi; \
	done

# Compile .po into .mo under build/ (no writes into $HOME).
mo:
	@for lang in $(LINGUAS); do \
		mkdir -p $(LOCALEDIR_BUILD)/$$lang/LC_MESSAGES; \
		msgfmt $(PODIR)/$$lang.po \
			-o $(LOCALEDIR_BUILD)/$$lang/LC_MESSAGES/digital-wellbeing.mo; \
	done

locales: pot po mo

install: $(TARGET) mo
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@for lang in $(LINGUAS); do \
		install -Dm644 \
			$(LOCALEDIR_BUILD)/$$lang/LC_MESSAGES/digital-wellbeing.mo \
			$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/digital-wellbeing.mo; \
	done

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@for lang in $(LINGUAS); do \
		rm -f $(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/digital-wellbeing.mo; \
	done
