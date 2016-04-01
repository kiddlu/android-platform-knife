LOCAL_PATH:= $(call my-dir)

libpcre_src_files := \
    pcre_chartables.c \
    dist/pcre_byte_order.c \
    dist/pcre_compile.c \
    dist/pcre_config.c \
    dist/pcre_dfa_exec.c \
    dist/pcre_exec.c \
    dist/pcre_fullinfo.c \
    dist/pcre_get.c \
    dist/pcre_globals.c \
    dist/pcre_jit_compile.c \
    dist/pcre_maketables.c \
    dist/pcre_newline.c \
    dist/pcre_ord2utf8.c \
    dist/pcre_refcount.c \
    dist/pcre_string_utils.c \
    dist/pcre_study.c \
    dist/pcre_tables.c \
    dist/pcre_ucd.c \
    dist/pcre_valid_utf8.c \
    dist/pcre_version.c \
    dist/pcre_xclass.c

include $(CLEAR_VARS)
LOCAL_MODULE := libpcre
LOCAL_CFLAGS += -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(LOCAL_PATH)/dist
LOCAL_SRC_FILES := $(libpcre_src_files)
include $(BUILD_HOST_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libpcre
LOCAL_CFLAGS += -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(LOCAL_PATH)/dist
LOCAL_SRC_FILES := $(libpcre_src_files)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libpcre
LOCAL_CFLAGS += -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(LOCAL_PATH)/dist
LOCAL_SRC_FILES := $(libpcre_src_files)
include $(BUILD_SHARED_LIBRARY)

