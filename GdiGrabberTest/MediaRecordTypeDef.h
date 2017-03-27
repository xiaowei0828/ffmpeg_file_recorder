#ifndef MEDIARECORDTYPEDEF_H
#define MEDIARECORDTYPEDEF_H

namespace MediaFileRecorder
{
	enum PIX_FMT
	{
		PIX_FMT_RGB24,
		PIX_FMT_BGR24,
		PIX_FMT_ARGB,
		PIX_FMT_BGRA,
	};

	enum AUDIO_FORMAT
	{
		AUDIO_FORMAT_UNKNOWN,

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
		SPEAKERS_UNKNOWN,
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

	struct AUDIO_INFO
	{
		int sample_rate;
		AUDIO_FORMAT audio_format;
		CHANNEL_LAYOUT chl_layout;
	};


	struct RecordInfo
	{
		char file_name[1024];
		struct Video
		{
			int dst_width;
			int dst_height;
			int frame_rate;
			int bit_rate;
		}video_info;

		struct Audio
		{
		}audio_info;

		RecordInfo()
		{
			memset(file_name, 0, 1024);
			video_info.dst_width = 0;
			video_info.dst_height = 0;
			video_info.frame_rate = 0;
			video_info.bit_rate = 0;
		}
	};

}
#endif