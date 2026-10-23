CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wpedantic
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags freetype2 libevdev)
LDLIBS += $(shell $(PKG_CONFIG) --libs freetype2 libevdev)

fbkeyboard: fbkeyboard.o touch.o

fbkeyboard.o touch.o: touch.h

test-touch: test_touch.c touch.c touch.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_touch.c touch.c

test-keyboard: test_keyboard.c fbkeyboard.c touch.c touch.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ test_keyboard.c touch.c $(LDLIBS)

check: test-touch test-keyboard
	./test-touch
	./test-keyboard

install: fbkeyboard
	install -Dm755 fbkeyboard $(DESTDIR)/usr/bin/fbkeyboard

clean:
	rm -f fbkeyboard test-touch test-keyboard *.o

.PHONY: check install clean

