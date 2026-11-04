##### Project #####

PROJECT        ?= app
# Top path of the template
TOP             = .
# The path for generated files
BUILD_DIR       = _build

##### Options #####
LDSCRIPT        = vendor/Ld/Link.ld

# Verified USBHS throughput profile. These remain overridable for experiments.
USBHS_EP2_BURST_BYTES       ?= 65024
USBHS_EP2_PAYLOAD_LIMIT     ?= 512
FILEXFER_ROW_BUF_CNT        ?= 32
FILEXFER_RAM_GENERATOR      ?= 1
FILEXFER_PACKED_GENERATOR   ?= 1
FILEXFER_STATIC_PATTERN     ?= 0
OPT                        ?= -O3 -Wextra
LIB_FLAGS = DEBUG=0 \
    USBHS_EP2_BURST_BYTES=$(USBHS_EP2_BURST_BYTES) \
    USBHS_EP2_PAYLOAD_LIMIT=$(USBHS_EP2_PAYLOAD_LIMIT) \
    FILEXFER_ROW_BUF_CNT=$(FILEXFER_ROW_BUF_CNT) \
    FILEXFER_RAM_GENERATOR=$(FILEXFER_RAM_GENERATOR) \
    FILEXFER_PACKED_GENERATOR=$(FILEXFER_PACKED_GENERATOR) \
    FILEXFER_STATIC_PATTERN=$(FILEXFER_STATIC_PATTERN)

# version == file with number looks like 1015
VERSION_FILE    = version
CONTENTS		= $$(cat $(VERSION_FILE))

ifndef SW_VERSION
# if sw_version is not written by param - then get it from file
SW_VERSION 	= $$(echo $(CONTENTS) | cut -d'.' -f1 | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/')
endif

DT_UNIX 	?= $(shell date +%s)

# GCC 15: riscv32-wch-elf-
# GCC 12: riscv-none-elf-
# GCC 11 and below: riscv-none-embed-
GCC_PREFIX      ?= riscv32-wch-elf-

# C source folders
CDIRS := app \
vendor/RVMSIS \
vendor/StdPeriphDriver

# C source files (if there are any single ones)
CFILES  := \

# ASM source folders
ADIRS   :=
# ASM single files
AFILES  = vendor/Startup/startup_CH585.S

LIBS    = vendor/StdPeriphDriver

LIBNAMES = ISP585 \
		printf


# Include paths
INCLUDES := \
app \
vendor \
vendor/RVMSIS \
vendor/StdPeriphDriver/inc

##### Optional Libraries ############

include ./rules.mk
