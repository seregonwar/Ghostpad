# SPDX-License-Identifier: GPL-3.0-or-later
#
#  ____  _               _                     _   ____   ____  _  __
# / ___|| |__   ___  ___| |_ _ __   __ _  __| | |  _ \ / ___|| |/ /
# | |  _ | '_ \ / _ \/ __| __| '_ \ / _` |/ _` | | |_) |\___ \| ' / 
# | |_| || | | | (_) \__ \ |_| |_) | (_| | (_| | |  __/  ___) | . \ 
#  \____||_| |_|\___/|___/\__| .__/ \__,_|\__,_| |_|    |____/|_|\_\
#                            |_|                                    
#

PS4_HOST    ?= ps4
PS5_HOST    ?= ps5
PORT        ?= 9021

# ── TARGET CONFIGURATION ──────────────────────────────────────────────────

ifdef PS4_PAYLOAD_SDK
    include $(PS4_PAYLOAD_SDK)/toolchain/orbis.mk
    CFLAGS  += -D__ORBIS__
    TARGET  := ghostpad.elf
    DEPLOY  := $(PS4_DEPLOY)
    HOST    := $(PS4_HOST)
else ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
    CFLAGS  += -D__PROSPERO__
    TARGET  := ghostpad.elf
    DEPLOY  := $(PS5_PAYLOAD_SDK)/bin/prospero-deploy
    HOST    := $(PS5_HOST)
else
    $(error Neither PS4_PAYLOAD_SDK nor PS5_PAYLOAD_SDK is defined)
endif

# ── COMPILATION FLAGS ─────────────────────────────────────────────────────

CFLAGS  += -Wall -Wextra -g -O2 -fPIC -fno-stack-protector
LDFLAGS += -lScePad -lSceUserService -lpthread -ldl

SRC     := main.c shellui_pad.c

# ── RULES ─────────────────────────────────────────────────────────────────

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	$(DEPLOY) -h $(HOST) -p $(PORT) $<
