
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
	$(TOP_DIR)/jni
LOCAL_SRC_FILES := \
	common/network.c \
	common/netopts.c \
	common/message.c \
	common/unix.c \

ifdef USE_OPENSSL
LOCAL_SRC_FILES += \
	common/openssl.c
LOCAL_STATIC_LIBRARIES := libssl_static libcrypto_static
#LOCAL_SHARED_LIBRARIES := libssl$(OPENSSL_POSTFIX) libcrypto$(OPENSSL_POSTFIX)
else
LOCAL_SRC_FILES += \
	common/nullenc.c
endif

include $(BUILD_STATIC_LIBRARY)

ifdef BUILD_PCSCLITE

#
# Shared libpcsclite library
#
include $(CLEAR_VARS)

LOCAL_MODULE    := libpcsclite
LOCAL_CFLAGS	:= -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := \
	$(TOP_DIR)/jni \
	$(common_src_include)

LOCAL_SRC_FILES := \
	client/client.c

LOCAL_LDLIBS	:= -llog

LOCAL_STATIC_LIBRARIES := libcommon

ifdef USE_OPENSSL
LOCAL_STATIC_LIBRARIES += libssl_static libcrypto_static
#LOCAL_SHARED_LIBRARIES := libssl$(OPENSSL_POSTFIX) libcrypto$(OPENSSL_POSTFIX)
endif

include $(BUILD_SHARED_LIBRARY)

endif

#
# Shared libpcscproxy library
#
include $(CLEAR_VARS)

LOCAL_MODULE    := libpcscproxy
LOCAL_CFLAGS	:= -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := \
	$(TOP_DIR)/jni \
	$(common_src_include)

LOCAL_SRC_FILES := \
	server/server.c

LOCAL_LDLIBS	:= -llog

LOCAL_STATIC_LIBRARIES := libcommon
LOCAL_SHARED_LIBRARIES := libpcsclite

ifdef USE_OPENSSL
LOCAL_SHARED_LIBRARIES += libssl$(OPENSSL_POSTFIX) libcrypto$(OPENSSL_POSTFIX)
endif

include $(BUILD_SHARED_LIBRARY)

ifdef USE_OPENSSL
$(call import-module,openssl)
endif
