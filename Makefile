CC = gcc
CFLAGS = -O2 -DNDEBUG -fstrict-aliasing -flto -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -ffast-math -fvisibility=hidden -fuse-ld=gold -Wl,--gc-sections,--build-id=none,-O1 -s `pkg-config --cflags --libs gtk+-2.0` -Wno-deprecated-declarations
LDFLAGS = `pkg-config --libs gtk+-2.0`

ICON_PATH = ./icons

all: convert_icons umbatts

battery_icons.h: convert_images.py
	python3 convert_images.py $(ICON_PATH)

umbatts: umbatts.c battery_icons.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

convert_icons: convert_images.py
	python3 convert_images.py $(ICON_PATH)

clean:
	rm -f umbatts battery_icons.h

.PHONY: all clean convert_icons
