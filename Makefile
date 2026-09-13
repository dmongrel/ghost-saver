# ghost-saver Makefile
# Builds a Windows screensaver (.scr file) that cycles through colors
# to help remove ghosting/burn-in artifacts from monitors.

CXX      = g++
WINDRES  = windres
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -DUNICODE -D_UNICODE
LDFLAGS  = -mwindows -static
LDLIBS   = -lgdi32 -lshell32
TARGET   = ghost-saver.scr
RES      = ghost-saver.res

.PHONY: all clean install

all: $(TARGET)

$(RES): ghost-saver.rc ghost-saver.manifest
	$(WINDRES) --preprocessor="cat" ghost-saver.rc -O coff -o $@

$(TARGET): main.cpp $(RES)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(RES) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(RES)

install: $(TARGET)
	cp $(TARGET) "$(SYSTEMROOT)/System32/"
