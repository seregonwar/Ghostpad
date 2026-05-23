# SPDX-License-Identifier: GPL-3.0-or-later
# Ghostpad PS5 Payload Makefile
#
# Usage:
#   make                          - Build ghostpad.elf
#   make clean                    - Remove build artifacts
#   make test PS5_HOST=<ip>       - Deploy to PS5 (ELF loader on port 9021)
#
# Requirements:
#   PS5_PAYLOAD_SDK environment variable pointing to installed SDK
#   e.g. export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

PS5_HOST    ?= ps5
PS5_PORT    ?= 9021

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

CFLAGS  += -Wall -Wextra -g -O2 -fPIC -fno-stack-protector
LDFLAGS += -lScePad -lSceUserService -lpthread

TARGET  := ghostpad.elf
SRC     := main.c shellui_pad.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	$(PS5_PAYLOAD_SDK)/bin/prospero-deploy -h $(PS5_HOST) -p $(PS5_PORT) $<
