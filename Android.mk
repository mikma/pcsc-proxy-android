
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

LOCAL_STATIC_LIBRARIES := libssl_static libcrypto_static
#LOCAL_SHARED_LIBRARIES := libssl$(OPENSSL_POSTFIX) libcrypto$(OPENSSL_POSTFIX)

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

LOCAL_STATIC_LIBRARIES := libcommon
LOCAL_STATIC_LIBRARIES += libssl_static libcrypto_static
#LOCAL_SHARED_LIBRARIES := libssl$(OPENSSL_POSTFIX) libcrypto$(OPENSSL_POSTFIX)

include $(BUILD_SHARED_LIBRARY)

$(call import-module,openssl)
