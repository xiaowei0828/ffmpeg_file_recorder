LOCAL_PATH := $(call my-dir)

#$(error LOCAL_PATH is: $(NDK_ROOT)/platforms/$(APP_PLATFORM)/$(ARCH_TARGET))

include $(CLEAR_VARS)

MY_SRC_FILES :=     source/aligned_malloc.cc \
                    source/clock.cc \
                    source/condition_variable.cc \
                    source/condition_variable_posix.cc \
                    source/cpu_features.cc \
                    source/cpu_info.cc \
                    source/critical_section.cc \
                    source/critical_section_posix.cc \
                    source/data_log_c.cc \
                    source/event.cc \
                    source/event_timer_posix.cc \
                    source/event_tracer.cc \
                    source/file_impl.cc \
                    source/logging.cc \
                    source/rtp_to_ntp.cc \
                    source/rw_lock.cc \
                    source/rw_lock_generic.cc \
                    source/rw_lock_posix.cc \
                    source/sleep.cc \
                    source/thread.cc \
                    source/thread_posix.cc \
                    source/tick_util.cc \
                    source/timestamp_extrapolator.cc \
                    source/trace_impl.cc \
                    source/trace_posix.cc \
                    source/atomic32_posix.cc \
                    source/data_log_no_op.cc \
                    source/logcat_trace_context.cc \
                    source/cpu_features_android.c \
                    source/metrics_default.cc
                    #source/sort.cc \
                    
LOCAL_MODULE := system_wrappers

LOCAL_C_INCLUDES := $(REPO_ROOT_DIR)

LOCAL_SRC_FILES := $(MY_SRC_FILES)

LOCAL_CFLAGS :=   $(MY_MACROS) $(MY_LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(MY_MACROS) $(MY_LOCAL_CPPFLAGS)

#LOCAL_STATIC_LIBRARIES := cpufeatures
#LOCAL_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)