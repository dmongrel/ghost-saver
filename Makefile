# ghost-saver Makefile
# Builds a Windows screensaver (.scr file) that cycles through colors
# to help remove ghosting/burn-in artifacts from monitors.

CC = g++
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -mwindows -lgdi32
TARGET = ghost-saver.scr
SRC = main.cpp

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) "$(windir)\System32\"
