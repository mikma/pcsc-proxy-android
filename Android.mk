
TOP_DIR := $(call my-dir)
LOCAL_PATH := $(TOP_DIR)/src

common_src_include := $(LOCAL_PATH)/common

#
# Static libcommon library
#
include $(CLEAR_VARS)

LOCAL_MODULE    := libcommon
LOCAL_CFLAGS	:= -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := \
	jni
LOCAL_SRC_FILES := \
	common/network.c \
	common/netopts.c \
	common/message.c \
	common/openssl.c \
	common/unix.c \

LOCAL_SHARED_LIBRARIES := libssl libcrypto

include $(BUILD_STATIC_LIBRARY)


#
# Shared libpcsclite library
#
include $(CLEAR_VARS)

LOCAL_MODULE    := libpcsclite
LOCAL_CFLAGS	:= -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := \
	jni \
	$(common_src_include)

LOCAL_SRC_FILES := \
	client/client.c

LOCAL_LDLIBS	:= -llog

LOCAL_SHARED_LIBRARIES := libcommon libssl libcrypto

include $(BUILD_SHARED_LIBRARY)

$(call import-module,openssl)
