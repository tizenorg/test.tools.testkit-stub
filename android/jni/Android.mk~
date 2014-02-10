LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -fexceptions
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_MODULE := testkit-stub
LOCAL_SRC_FILES := ../../source/comfun.cpp ../../source/httpserver.cpp ../../source/testcase.cpp ../../source/main/main.cpp ../../source/json/json_reader.cpp ../../source/json/json_value.cpp ../../source/json/json_writer.cpp
include $(BUILD_EXECUTABLE)
