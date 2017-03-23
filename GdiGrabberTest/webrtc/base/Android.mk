LOCAL_PATH := $(call my-dir)

#$(error LOCAL_PATH is: $(LOCAL_PATH))

include $(CLEAR_VARS)

MY_SRC_FILES :=     bitbuffer.cc \
                    buffer.cc \
                    bufferqueue.cc \
                    bytebuffer.cc \
                    checks.cc \
                    criticalsection.cc \
                    event.cc \
                    event_tracer.cc \
                    exp_filter.cc \
                    md5.cc \
                    md5digest.cc \
                    platform_file.cc \
                    platform_thread.cc \
                    stringencode.cc \
                    stringutils.cc \
                    systeminfo.cc \
                    thread_checker_impl.cc \
                    timeutils.cc \
                    logging.cc \
                    android/CallStack.cpp
					
LOCAL_MODULE := webrtc_base

LOCAL_C_INCLUDES := $(REPO_ROOT_DIR)
#LOCAL_SRC_FILES += $(addprefix $(LOCAL_PATH)/, $(MY_SRC_FILES))
LOCAL_SRC_FILES := $(MY_SRC_FILES)

LOCAL_CFLAGS :=   $(MY_MACROS) $(MY_LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(MY_MACROS) $(MY_LOCAL_CPPFLAGS)

LOCAL_STATIC_LIBRARIES :=
LOCAL_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)
