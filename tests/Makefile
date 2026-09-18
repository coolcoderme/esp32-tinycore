LIB_SRCS = \
	../bootloader/lib/mbr.c \
	../bootloader/lib/gpt.c \
	../bootloader/lib/part.c \
	../bootloader/lib/fat.c \
	../bootloader/lib/cfg.c \
	../bootloader/lib/detect.c \
	../bootloader/lib/layout.c \
	../bootloader/lib/fdtpatch.c

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -g
CPPFLAGS += -I../bootloader/include
SANITIZE ?=

.PHONY: all test clean

all: test_sdboot

test_sdboot: test_sdboot.c $(LIB_SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZE) -o $@ test_sdboot.c $(LIB_SRCS)

test: test_sdboot
	./test_sdboot $(DTB) $(IMG)

clean:
	rm -f test_sdboot
