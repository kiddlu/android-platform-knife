LOCAL_PATH:= $(call my-dir)

###################################
include $(CLEAR_VARS)

LOCAL_MODULE := sepolicy-analyze
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := external/libsepol/include
LOCAL_CFLAGS := -Wall -Werror
LOCAL_SRC_FILES := sepolicy-analyze.c dups.c neverallow.c perm.c typecmp.c utils.c
LOCAL_STATIC_LIBRARIES := libsepol

include $(BUILD_HOST_EXECUTABLE)
