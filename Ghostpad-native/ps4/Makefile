# Ghostpad PS4 Homebrew — OpenOrbis Makefile
# Run from Ghostpad-native/ps4/ directory
# Builds eboot.bin and optionally packages into .pkg

PROJECT_ROOT := $(shell cd ../../.. && pwd)
GHOSTPAD_ROOT := $(shell cd .. && pwd)

OO_PS4_TOOLCHAIN ?= /opt/OpenOrbis-PS4-Toolchain
LLVM_PREFIX := $(shell ls -d /opt/homebrew/opt/llvm 2>/dev/null || ls -d /usr/local/opt/llvm 2>/dev/null || echo /usr)
OO_CDIR := $(shell uname -s | tr '[:upper:]' '[:lower:]' | sed 's/darwin/macos/')

OO_CC        := $(LLVM_PREFIX)/bin/clang
OO_CXX       := $(LLVM_PREFIX)/bin/clang++
OO_LD        := $(LLVM_PREFIX)/bin/ld.lld
OO_CREATEGP4 := $(OO_PS4_TOOLCHAIN)/bin/$(OO_CDIR)/create-gp4
OO_PKGTOOL   := $(OO_PS4_TOOLCHAIN)/bin/$(OO_CDIR)/PkgTool.Core
CREATE_FSELF := $(OO_PS4_TOOLCHAIN)/bin/$(OO_CDIR)/create-fself

TARGET       := ghostpad
TITLE_ID     := BREW00085
TITLE        := Ghostpad PS4
VERSION      := 1.00
CONTENT_ID   := IV0000-BREW00085_00-GHOSTPADPS4HMBRE

BUILD_DIR    := build
EBOOT_BIN    := eboot.bin
HOMEBREW_ELF := $(BUILD_DIR)/$(TARGET).elf

# ── Compiler flags ────────────────────────────────────────────────
TARGET_TRIPLE := x86_64-pc-freebsd12-elf

CFLAGS   := --target=$(TARGET_TRIPLE) -fPIC -funwind-tables -O3 -fno-strict-aliasing -ffunction-sections -fdata-sections
CFLAGS   += -isysroot $(OO_PS4_TOOLCHAIN)
CFLAGS   += -isystem $(OO_PS4_TOOLCHAIN)/include
CFLAGS   += -isystem $(OO_PS4_TOOLCHAIN)/include/orbis
CFLAGS   += -D__ORBIS__

CXXFLAGS := $(CFLAGS) -std=c++17
CXXFLAGS += -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1
CXXFLAGS += -stdlib=libc++
CXXFLAGS += -include ps4_compat.h
CXXFLAGS += -D_LIBCPP_HAS_NO_LOCALIZATION
CXXFLAGS += -D_LIBCPP_HAS_NO_WIDE_CHARACTERS
CXXFLAGS += -DIMGUI_DEFINE_MATH_OPERATORS -DGHOSTPAD_CONSOLE

# ── Linker flags ──────────────────────────────────────────────────
LDFLAGS  := -m elf_x86_64 -pie
LDFLAGS  += --script $(OO_PS4_TOOLCHAIN)/link.x
LDFLAGS  += --eh-frame-hdr
LDFLAGS  += -L$(OO_PS4_TOOLCHAIN)/lib

LIBS     := -lc -lc++ -lkernel -lSceVideoOut -lSceSysmodule -lScePad -lSceUserService

# ── Source directories ────────────────────────────────────────────
GP_SRC      := $(GHOSTPAD_ROOT)/src
GP_INPUT    := $(GP_SRC)/input
GP_NETWORK  := $(GP_SRC)/network
GP_PROTOCOL := $(GP_SRC)/protocol
GP_STORAGE  := $(GP_SRC)/storage
GP_UI       := $(GP_SRC)/ui

IMGUI_DIR   := $(GHOSTPAD_ROOT)/build/_deps/imgui-src
JSON_DIR    := $(GHOSTPAD_ROOT)/build/_deps/json-src/single_include

LOCAL_DIR   := .

# ── Ghostpad C++ sources ──────────────────────────────────────────
GP_SRCS := \
    $(GP_INPUT)/keyboard_input.cpp \
    $(GP_INPUT)/macro_engine.cpp \
    $(GP_NETWORK)/beeper_client.cpp \
    $(GP_NETWORK)/ghostpad_client.cpp \
    $(GP_NETWORK)/network_scanner.cpp \
    $(GP_NETWORK)/payload_deploy.cpp \
    $(GP_NETWORK)/ssm_client.cpp \
    $(GP_STORAGE)/console_store.cpp \
    $(GP_STORAGE)/profile_store.cpp \
    $(GP_STORAGE)/project_store.cpp \
    $(GP_STORAGE)/settings_store.cpp \
    $(GP_UI)/app.cpp \
    $(GP_UI)/beeper_screen.cpp \
    $(GP_UI)/consoles_screen.cpp \
    $(GP_UI)/controller_screen.cpp \
    $(GP_UI)/credits_screen.cpp \
    $(GP_UI)/file_picker.cpp \
    $(GP_UI)/gif_export.cpp \
    $(GP_UI)/home_screen.cpp \
    $(GP_UI)/input_redirect_screen.cpp \
    $(GP_UI)/pad_visualizer.cpp \
    $(GP_UI)/projects_screen.cpp \
    $(GP_UI)/settings_screen.cpp \
    $(GP_UI)/systemstate_screen.cpp

# PS4 stubs + entry point
LOCAL_SRCS := \
    $(LOCAL_DIR)/main_ps4.cpp \
    $(LOCAL_DIR)/stubs/glfw_console_stubs.cpp \
    $(LOCAL_DIR)/stubs/imgui_console_stubs.cpp \
    $(LOCAL_DIR)/stubs/gamepad_console_stub.cpp

# ImGui core
IMGUI_SRCS := \
    $(IMGUI_DIR)/imgui.cpp \
    $(IMGUI_DIR)/imgui_draw.cpp \
    $(IMGUI_DIR)/imgui_tables.cpp \
    $(IMGUI_DIR)/imgui_widgets.cpp

GP_OBJS    := $(patsubst $(GP_SRC)/%.cpp,$(BUILD_DIR)/gp/%.o,$(GP_SRCS))
LOCAL_OBJS := build/imgui_impl_ps4_sw.o $(patsubst $(LOCAL_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LOCAL_SRCS))
IMGUI_OBJS := $(patsubst $(IMGUI_DIR)/%.cpp,$(BUILD_DIR)/imgui/%.o,$(IMGUI_SRCS))

ALL_OBJS := $(GP_OBJS) $(LOCAL_OBJS) $(IMGUI_OBJS)

# ── Include paths ─────────────────────────────────────────────────
INCLUDES := \
    -I$(GP_SRC) \
    -I$(GP_INPUT) \
    -I$(GP_NETWORK) \
    -I$(GP_PROTOCOL) \
    -I$(GP_STORAGE) \
    -I$(GP_UI) \
    -I$(IMGUI_DIR) \
    -I$(IMGUI_DIR)/backends \
    -I$(JSON_DIR) \
    -I$(LOCAL_DIR) \
    -I$(LOCAL_DIR)/stubs \
    -I$(LOCAL_DIR)/assets

# ── Build targets ─────────────────────────────────────────────────
.PHONY: all clean pkg

all: $(EBOOT_BIN)

# Compile Ghostpad C++
$(BUILD_DIR)/gp/%.o: $(GP_SRC)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  [CXX]  $(notdir $<)"
	$(OO_CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile PS4 stubs
$(BUILD_DIR)/%.o: $(LOCAL_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  [CXX]  $(notdir $<)"
	$(OO_CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ImGui PS4 Software Rasterizer backend
$(BUILD_DIR)/imgui_impl_ps4_sw.o: imgui_impl_ps4_sw.cpp imgui_impl_ps4_sw.h
	@mkdir -p $(BUILD_DIR)
	@echo "  [CXX]  imgui_impl_ps4_sw.cpp"
	$(OO_CXX) $(CXXFLAGS) -I$(IMGUI_DIR) -c $< -o $@

# Compile ImGui
$(BUILD_DIR)/imgui/imgui.o: $(IMGUI_DIR)/imgui.cpp
	@mkdir -p $(dir $@)
	@echo "  [CXX]  imgui/imgui.cpp"
	$(OO_CXX) $(CXXFLAGS) -I$(IMGUI_DIR) -c $< -o $@

$(BUILD_DIR)/imgui/imgui_draw.o: $(IMGUI_DIR)/imgui_draw.cpp
	@mkdir -p $(dir $@)
	@echo "  [CXX]  imgui/imgui_draw.cpp"
	$(OO_CXX) $(CXXFLAGS) -I$(IMGUI_DIR) -c $< -o $@

$(BUILD_DIR)/imgui/imgui_tables.o: $(IMGUI_DIR)/imgui_tables.cpp
	@mkdir -p $(dir $@)
	@echo "  [CXX]  imgui/imgui_tables.cpp"
	$(OO_CXX) $(CXXFLAGS) -I$(IMGUI_DIR) -c $< -o $@

$(BUILD_DIR)/imgui/imgui_widgets.o: $(IMGUI_DIR)/imgui_widgets.cpp
	@mkdir -p $(dir $@)
	@echo "  [CXX]  imgui/imgui_widgets.cpp"
	$(OO_CXX) $(CXXFLAGS) -I$(IMGUI_DIR) -c $< -o $@

# Link
$(HOMEBREW_ELF): $(ALL_OBJS)
	@echo "  [LD]   $(notdir $@)"
	$(OO_LD) $(LDFLAGS) $(ALL_OBJS) $(LIBS) $(OO_PS4_TOOLCHAIN)/lib/crt1.o -o $@

# Fake self (via create-fself)
# --paid 0x3800000000000013 = PS4 Big App authid (correct for homebrew)
$(EBOOT_BIN): $(HOMEBREW_ELF)
	@echo "  [FSELF] Creating eboot.bin..."
	$(CREATE_FSELF) -in=$(HOMEBREW_ELF) -out=$(BUILD_DIR)/$(TARGET).oelf --eboot "$(EBOOT_BIN)" --paid 0x3800000000000013

# ── PKG targets ───────────────────────────────────────────────────
pkg: $(EBOOT_BIN) sce_sys/param.sfo sce_sys/icon0.png pkg.gp4
	@echo "  [PKG]  $(CONTENT_ID).pkg"
	$(OO_PKGTOOL) pkg_build pkg.gp4 .
	@echo "  DONE: $(CONTENT_ID).pkg"

sce_sys/param.sfo: Makefile
	@echo "  [SFO]    $@"
	$(OO_PKGTOOL) sfo_new $@
	$(OO_PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(OO_PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(OO_PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0x00000000
	$(OO_PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(OO_PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(OO_PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(OO_PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(OO_PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(OO_PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(OO_PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

# PRX modules for sce_module (required by PS4 kernel)
sce_module/libc.prx:
	@mkdir -p sce_module
	@cp $(OO_PS4_TOOLCHAIN)/src/modules/libc.prx $@

sce_module/libSceFios2.prx:
	@mkdir -p sce_module
	@cp $(OO_PS4_TOOLCHAIN)/src/modules/libSceFios2.prx $@

pkg.gp4: $(EBOOT_BIN) sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx
	@echo "  [GP4]  pkg.gp4"
	$(OO_CREATEGP4) -out $@ --content-id=$(CONTENT_ID) --files "$^"

clean:
	rm -rf $(BUILD_DIR) $(EBOOT_BIN) pkg.gp4 sce_sys/param.sfo $(CONTENT_ID).pkg __pycache__
