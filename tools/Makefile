CC ?= cc
CFLAGS ?= -O3 -g
OUT := ../ch585m/_build/tools
USB_CFLAGS := $(shell pkg-config --cflags libusb-1.0)
USB_LIBS := $(shell pkg-config --libs libusb-1.0)
.PHONY: all check
all: $(OUT)/bmp-bench $(OUT)/v208-controls
$(OUT)/v208-controls: v208_controls.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror -Wpedantic $(USB_CFLAGS) $< $(USB_LIBS) -o $@
$(OUT)/bmp-bench: bmp_bench.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror -Wpedantic $(USB_CFLAGS) $< $(USB_LIBS) -o $@
check: all
	python3 test_filexfer.py
	$(CC) -std=gnu11 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
	    -I../ch32v208wb/app ../ch32v208wb/tests/core_test.c ../ch32v208wb/app/demo.c -o $(OUT)/v208-core-test
	$(OUT)/v208-core-test
