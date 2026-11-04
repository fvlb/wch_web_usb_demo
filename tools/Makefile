CC ?= cc
CFLAGS ?= -O3 -g
OUT := ../ch585m/_build/tools
USB_CFLAGS := $(shell pkg-config --cflags libusb-1.0)
USB_LIBS := $(shell pkg-config --libs libusb-1.0)
.PHONY: all check
all: $(OUT)/bmp-bench
$(OUT)/bmp-bench: bmp_bench.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -Werror -Wpedantic $(USB_CFLAGS) $< $(USB_LIBS) -o $@
check: all
	python3 test_filexfer.py
