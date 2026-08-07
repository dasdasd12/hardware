CC = gcc
BUILD_ROOT ?= build
ROOT := ../..
FIRMWARE := $(ROOT)/firmware

CFLAGS := -std=gnu99 -Wall -Wextra
COMMON_INC := -I$(FIRMWARE)/common -I$(FIRMWARE)/h417/v3f/applications
STUB_INC := -Istubs

PROGRAMS := \
	$(BUILD_ROOT)/host_approval_control_test.exe \
	$(BUILD_ROOT)/host_approval_mailbox_test.exe \
	$(BUILD_ROOT)/host_ch585_link_test.exe \
	$(BUILD_ROOT)/host_pc_link_test.exe \
	$(BUILD_ROOT)/host_profile_shortcut_test.exe \
	$(BUILD_ROOT)/host_profile_runtime_test.exe \
	$(BUILD_ROOT)/host_profile_link_half0.exe \
	$(BUILD_ROOT)/host_profile_link_half1.exe

.PHONY: all run clean

all: $(PROGRAMS)

run: all
	@"$(BUILD_ROOT)/host_approval_control_test.exe"
	@"$(BUILD_ROOT)/host_approval_mailbox_test.exe"
	@"$(BUILD_ROOT)/host_ch585_link_test.exe"
	@"$(BUILD_ROOT)/host_pc_link_test.exe"
	@"$(BUILD_ROOT)/host_profile_shortcut_test.exe"
	@"$(BUILD_ROOT)/host_profile_runtime_test.exe"
	@"$(BUILD_ROOT)/host_profile_link_half0.exe"
	@"$(BUILD_ROOT)/host_profile_link_half1.exe"

$(BUILD_ROOT):
	@mkdir -p "$@"

$(BUILD_ROOT)/host_approval_control_test.exe: host_approval_control_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) $(COMMON_INC) $< \
		$(FIRMWARE)/h417/v3f/applications/rf_report_bridge.c -o "$@"

$(BUILD_ROOT)/host_approval_mailbox_test.exe: host_approval_mailbox_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) -I$(FIRMWARE)/common $< -o "$@"

$(BUILD_ROOT)/host_ch585_link_test.exe: host_ch585_link_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) -Werror $(STUB_INC) $(COMMON_INC) $< \
		$(FIRMWARE)/h417/v3f/applications/ch585_link.c -o "$@"

PC_LINK_SOURCES := \
	$(FIRMWARE)/h417/v3f/applications/pc_link.c \
	$(FIRMWARE)/h417/v3f/applications/profile_activate.c \
	$(FIRMWARE)/h417/v3f/applications/profile_runtime.c \
	$(FIRMWARE)/h417/v3f/applications/profile_sync.c \
	$(FIRMWARE)/h417/v3f/applications/factory_profile_image.c \
	$(FIRMWARE)/h417/v3f/applications/half_state.c

$(BUILD_ROOT)/host_pc_link_test.exe: host_pc_link_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) $(COMMON_INC) $< $(PC_LINK_SOURCES) -o "$@"

$(BUILD_ROOT)/host_profile_shortcut_test.exe: host_profile_shortcut_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) -Werror -I$(FIRMWARE)/common $< -o "$@"

PROFILE_RUNTIME_SOURCES := \
	$(FIRMWARE)/h417/v3f/applications/profile_runtime.c \
	$(FIRMWARE)/h417/v3f/applications/factory_profile_image.c \
	$(FIRMWARE)/h417/v3f/applications/half_state.c \
	$(FIRMWARE)/h417/v3f/applications/default_profile.c

$(BUILD_ROOT)/host_profile_runtime_test.exe: host_profile_runtime_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) $(COMMON_INC) $< $(PROFILE_RUNTIME_SOURCES) -o "$@"

PROFILE_LINK_SOURCES := \
	$(FIRMWARE)/ch585/applications/half_scan/ch585_profile.c \
	$(FIRMWARE)/ch585/applications/half_scan/ch585_half_report.c \
	$(FIRMWARE)/ch585/applications/magnetic_key_engine.c \
	$(FIRMWARE)/h417/v3f/applications/profile_runtime.c \
	$(FIRMWARE)/h417/v3f/applications/profile_sync.c \
	$(FIRMWARE)/h417/v3f/applications/factory_profile_image.c \
	$(FIRMWARE)/h417/v3f/applications/half_state.c

PROFILE_LINK_INC := $(STUB_INC) $(COMMON_INC) \
	-I$(FIRMWARE)/ch585/applications \
	-I$(FIRMWARE)/ch585/applications/half_scan

$(BUILD_ROOT)/host_profile_link_half0.exe: host_profile_link_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) -DCH585_HALF_ID=0 $(PROFILE_LINK_INC) $< \
		$(PROFILE_LINK_SOURCES) -o "$@"

$(BUILD_ROOT)/host_profile_link_half1.exe: host_profile_link_test.c | $(BUILD_ROOT)
	$(CC) $(CFLAGS) -DCH585_HALF_ID=1 $(PROFILE_LINK_INC) $< \
		$(PROFILE_LINK_SOURCES) -o "$@"

clean:
	rm -rf "$(BUILD_ROOT)"
