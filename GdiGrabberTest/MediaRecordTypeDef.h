#ifndef MEDIARECORDTYPEDEF_H
#define MEDIARECORDTYPEDEF_H

namespace MediaFileRecorder
{
	enum PIX_FMT
	{
		PIX_FMT_UNKOWN = 0,
		PIX_FMT_RGB24,
		PIX_FMT_BGR24,
		PIX_FMT_ARGB,
		PIX_FMT_BGRA,
	};

	enum AUDIO_FORMAT
	{
		AUDIO_FORMAT_UNKNOWN = 0,

		AUDIO_FORMAT_U8BIT,
		AUDIO_FORMAT_16BIT,
		AUDIO_FORMAT_32BIT,
		AUDIO_FORMAT_FLOAT,

		AUDIO_FORMAT_U8BIT_PLANAR,
		AUDIO_FORMAT_16BIT_PLANAR,
		AUDIO_FORMAT_32BIT_PLANAR,
		AUDIO_FORMAT_FLOAT_PLANAR,
	};

	enum CHANNEL_LAYOUT
	{
		SPEAKERS_UNKNOWN = 0,
		SPEAKERS_MONO,
		SPEAKERS_STEREO,
		SPEAKERS_2POINT1,
		SPEAKERS_QUAD,
		SPEAKERS_4POINT1,
		SPEAKERS_5POINT1,
		SPEAKERS_5POINT1_SURROUND,
		SPEAKERS_7POINT1,
		SPEAKERS_7POINT1_SURROUND,
		SPEAKERS_SURROUND,
	};

	enum VIDEO_QUALITY
	{
		NORMAL = 0,
		HIGH,
		VERY_HIGH
	};

	struct VIDEO_INFO
	{
		int src_width;
		int src_height;
		PIX_FMT src_pix_fmt;
		int dst_width;
		int dst_height;
		int frame_rate;
		VIDEO_QUALITY quality;
		VIDEO_INFO()
		{
			memset(this, 0, sizeof(VIDEO_INFO));
		}
	};

	struct AUDIO_INFO
	{
		int sample_rate;
		AUDIO_FORMAT audio_format;
		CHANNEL_LAYOUT chl_layout;
		AUDIO_INFO()
		{
			memset(this, 0, sizeof(AUDIO_INFO));
		}
	};

	struct RECORD_INFO
	{
		char file_name[1024];
		bool is_record_video;
		bool is_record_mic;
		bool is_record_speaker;
		VIDEO_INFO video_info;
		AUDIO_INFO mic_audio_info;
		AUDIO_INFO speaker_audio_info;
		RECORD_INFO()
		{
			memset(file_name, 0, 1024);
			is_record_video = false;
			is_record_mic = false;
			is_record_speaker = false;
		}
	};

}
#endif