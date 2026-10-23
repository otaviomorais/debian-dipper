CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wpedantic
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags freetype2 libevdev)
LDLIBS += $(shell $(PKG_CONFIG) --libs freetype2 libevdev)

fbkeyboard: fbkeyboard.o touch.o haptics.o

fbkeyboard.o touch.o: touch.h
fbkeyboard.o haptics.o: haptics.h

test-touch: test_touch.c touch.c touch.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_touch.c touch.c

test-keyboard: test_keyboard.c fbkeyboard.c touch.c touch.h haptics.c haptics.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ test_keyboard.c touch.c haptics.c $(LDLIBS)

preview-keyboard: preview.c fbkeyboard.c touch.c touch.h haptics.c haptics.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ preview.c touch.c haptics.c $(LDLIBS)

test-haptics: test_haptics.c haptics.c haptics.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_haptics.c

check: test-touch test-keyboard test-haptics
	./test-touch
	./test-keyboard
	./test-haptics

install: fbkeyboard
	install -Dm755 fbkeyboard $(DESTDIR)/usr/bin/fbkeyboard

clean:
	rm -f fbkeyboard test-touch test-keyboard test-haptics preview-keyboard *.o

.PHONY: check install clean

