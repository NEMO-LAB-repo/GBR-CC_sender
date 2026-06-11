.RECIPEPREFIX := >

ANDROID_NDK_HOME ?= /home/qwu26/android-ndk-r26d
ANDROID_API ?= 21
DEVICE_PATH ?= /data/local/tmp/cellninjia_mobile

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    NDK_HOST_TAG ?= darwin-x86_64
else
    NDK_HOST_TAG ?= linux-x86_64
endif

CC := $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/$(NDK_HOST_TAG)/bin/aarch64-linux-android$(ANDROID_API)-clang
CFLAGS := -std=c99 -Werror -fPIE -pie
TARGET := cellninjia_mobile
SRC := cellninjia_mobile.c

all: $(TARGET)

$(TARGET): $(SRC)
>$(CC) $(CFLAGS) $< -o $@

# Compatibility alias for old muscle memory: the binary is now cellninjia_mobile.
diag_bridge: $(TARGET)
>@echo "diag_bridge has been renamed to $(TARGET)"

push: $(TARGET)
>adb push $(TARGET) $(DEVICE_PATH)
>adb shell chmod +x $(DEVICE_PATH)

clean:
>rm -f $(TARGET) diag_bridge

.PHONY: all diag_bridge push clean
