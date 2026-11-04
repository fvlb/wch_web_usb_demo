CROSS ?= riscv32-wch-elf-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
BUILD_DIR ?= _build
ARCH = -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -mno-save-restore
CPPFLAGS = -DCH32V20x_D8W -DDEBUG=1 -Iapp -Ivendor/Core -Ivendor/Debug -Ivendor/Peripheral/inc
CFLAGS = $(ARCH) $(CPPFLAGS) -O3 -g -std=gnu11 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common
SOURCES = $(wildcard app/*.c) vendor/Core/core_riscv.c vendor/Debug/debug.c \
    vendor/Peripheral/src/ch32v20x_rcc.c vendor/Peripheral/src/ch32v20x_gpio.c \
    vendor/Peripheral/src/ch32v20x_usart.c vendor/Peripheral/src/ch32v20x_tim.c \
    vendor/Peripheral/src/ch32v20x_misc.c
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(SOURCES)))
ifeq ($(REFERENCE),1)
BUILD_DIR = _build/evt-reference
CPPFLAGS := -Ivendor/usbfs-reference $(CPPFLAGS)
SOURCES := $(filter-out $(wildcard app/*.c),$(SOURCES)) $(wildcard vendor/usbfs-reference/*.c)
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(SOURCES)))
endif
.PHONY: all clean
.DELETE_ON_ERROR:
all: $(BUILD_DIR)/demo.bin $(BUILD_DIR)/demo.hex
$(BUILD_DIR)/%.o: %.c app/demo.h
	@mkdir -p $(BUILD_DIR)
	"$(CC)" $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD_DIR)/startup.o: vendor/Startup/startup_ch32v20x_D8W.S
	@mkdir -p $(BUILD_DIR)
	"$(CC)" $(ARCH) -g -c $< -o $@
$(BUILD_DIR)/demo.elf: $(OBJECTS) $(BUILD_DIR)/startup.o vendor/Ld/Link.ld
	"$(CC)" $(ARCH) -nostartfiles --specs=nano.specs --specs=nosys.specs \
	    -Wl,--gc-sections,--print-memory-usage,-Map,$(BUILD_DIR)/demo.map \
	    -Tvendor/Ld/Link.ld $(OBJECTS) $(BUILD_DIR)/startup.o -o $@
$(BUILD_DIR)/demo.bin: $(BUILD_DIR)/demo.elf
	"$(OBJCOPY)" -O binary $< $@
$(BUILD_DIR)/demo.hex: $(BUILD_DIR)/demo.elf
	"$(OBJCOPY)" -O ihex $< $@
clean:
	rm -rf $(BUILD_DIR)
-include $(OBJECTS:.o=.d)
