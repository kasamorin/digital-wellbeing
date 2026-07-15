.PHONY: all clean install uninstall

CC       := gcc
CXX      := g++

# pkg-config dependencies
C_PKGS   := json-c
CXX_PKGS := Qt6Widgets Qt6Gui Qt6Core

# C flags: gcc, C23
CFLAGS   := -std=gnu23 -Wall -Wextra -Wpedantic \
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

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(HOME)/.local/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(HOME)/.local/bin/$(TARGET)