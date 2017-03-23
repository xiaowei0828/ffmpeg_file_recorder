LOCAL_PATH := $(call my-dir)

#$(error LOCAL_PATH is: $(LOCAL_PATH))

include $(CLEAR_VARS)

MY_SRC_FILES :=     audio_converter.cc \
                    audio_ring_buffer.cc \
                    audio_util.cc \
                    blocker.cc \
                    channel_buffer.cc \
                    fft4g.c \
                    fir_filter.cc \
                    lapped_transform.cc \
                    real_fourier.cc \
                    real_fourier_ooura.cc \
                    resampler/push_resampler.cc \
                    resampler/push_sinc_resampler.cc \
                    resampler/resampler.cc \
                    resampler/sinc_resampler.cc \
                    ring_buffer.c \
                    signal_processing/auto_corr_to_refl_coef.c \
                    signal_processing/auto_correlation.c \
                    signal_processing/copy_set_operations.c \
                    signal_processing/cross_correlation.c \
                    signal_processing/division_operations.c \
                    signal_processing/dot_product_with_scale.c \
                    signal_processing/downsample_fast.c \
                    signal_processing/energy.c \
                    signal_processing/filter_ar.c \
                    signal_processing/filter_ma_fast_q12.c \
                    signal_processing/get_hanning_window.c \
                    signal_processing/get_scaling_square.c \
                    signal_processing/ilbc_specific_functions.c \
                    signal_processing/levinson_durbin.c \
                    signal_processing/lpc_to_refl_coef.c \
                    signal_processing/min_max_operations.c \
                    signal_processing/randomization_functions.c \
                    signal_processing/real_fft.c \
                    signal_processing/refl_coef_to_lpc.c \
                    signal_processing/resample.c \
                    signal_processing/resample_48khz.c \
                    signal_processing/resample_by_2.c \
                    signal_processing/resample_by_2_internal.c \
                    signal_processing/resample_fractional.c \
                    signal_processing/spl_init.c \
                    signal_processing/spl_sqrt.c \
                    signal_processing/splitting_filter.c \
                    signal_processing/sqrt_of_one_minus_x_squared.c \
                    signal_processing/vector_scaling_operations.c \
                    sparse_fir_filter.cc \
                    vad/vad.cc \
                    vad/vad_core.c \
                    vad/vad_filterbank.c \
                    vad/vad_gmm.c \
                    vad/vad_sp.c \
                    vad/webrtc_vad.c \
                    wav_file.cc \
                    wav_header.cc \
                    window_generator.cc \
                    signal_processing/complex_fft.c \
					signal_processing/complex_bit_reverse_arm.S \
					signal_processing/spl_sqrt_floor_arm.S 

ifeq ($(TARGET_ARCH_ABI), armeabi)
MY_SRC_FILES += signal_processing/filter_ar_fast_q12.c
else
MY_SRC_FILES += signal_processing/filter_ar_fast_q12_armv7.S \
			  fir_filter_neon.cc \
              resampler/sinc_resampler_neon.cc \
              signal_processing/cross_correlation_neon.c \
              signal_processing/downsample_fast_neon.c \
              signal_processing/min_max_operations_neon.c
endif
					
LOCAL_MODULE := common_audio

LOCAL_C_INCLUDES := $(REPO_ROOT_DIR)
#LOCAL_SRC_FILES += $(addprefix $(LOCAL_PATH)/, $(MY_SRC_FILES))
LOCAL_SRC_FILES := $(MY_SRC_FILES)

LOCAL_CFLAGS :=   $(MY_MACROS) $(MY_LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(MY_MACROS) $(MY_LOCAL_CPPFLAGS)

LOCAL_STATIC_LIBRARIES :=
LOCAL_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)
