LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := webrtc_modules_audio_device

LOCAL_C_INCLUDES := $(REPO_ROOT_DIR)
	
LOCAL_SRC_FILES :=  audio_device_impl.cc \
					audio_device_generic.cc \
					audio_device_buffer.cc \
					android/audio_manager.cc \
                    android/audio_record_jni.cc \
                    android/audio_track_jni.cc \
                    android/build_info.cc \
                    android/fine_audio_buffer.cc \
                    android/opensles_common.cc \
                    android/opensles_player.cc
                    
LOCAL_CFLAGS :=   $(MY_MACROS) $(MY_LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(MY_MACROS) $(MY_LOCAL_CPPFLAGS)

LOCAL_STATIC_LIBRARIES :=
LOCAL_LDLIBS := 

include $(BUILD_STATIC_LIBRARY)
