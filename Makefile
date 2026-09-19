.PHONY: all test test-asan img loader clean

PYTHON ?= python3
IMG ?= MicroCore-ESP32P4.img
DTB ?= tests/esp32p4-microcore-standalone.dtb

all: test img

tests/test_sdboot:
	$(MAKE) -C tests test_sdboot

$(DTB): dts/esp32p4-microcore-standalone.dts
	dtc -I dts -O dtb -o $@ $<

img $(IMG): dts/esp32p4-microcore-standalone.dts image/mkimg.py
	$(PYTHON) image/mkimg.py -o $(IMG)

test: tests/test_sdboot $(DTB) $(IMG)
	./tests/test_sdboot $(DTB) $(IMG)
	$(PYTHON) tests/test_image.py $(IMG)
	$(PYTHON) tests/test_netconf.py

test-asan: $(DTB) $(IMG)
	$(MAKE) -C tests clean
	$(MAKE) -C tests CC=gcc SANITIZE="-fsanitize=address,undefined" CFLAGS="-std=c11 -Wall -Wextra -Werror -O1 -g -fno-omit-frame-pointer" test_sdboot
	./tests/test_sdboot $(DTB) $(IMG)

loader:
	@if [ -z "$$IDF_PATH" ]; then \
		echo "IDF_PATH is not set. Install ESP-IDF v5.5.x and source export.sh"; \
		echo "then:  cd bootloader && idf.py set-target esp32p4 build"; \
		exit 1; \
	fi
	cd bootloader && idf.py set-target esp32p4 build

clean:
	$(MAKE) -C tests clean
	rm -f $(IMG) $(DTB)
	rm -rf bootloader/build
