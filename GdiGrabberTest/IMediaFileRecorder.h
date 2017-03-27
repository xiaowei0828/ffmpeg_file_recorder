#ifndef IMEDIAFILERCORDER_H
#define IMEDIAFILERCORDER_H

#include "MediaRecordTypeDef.h"

namespace MediaFileRecorder
{
	class IMediaFileRecorder
	{
	public:
		virtual int Init(const RecordInfo& record_info) = 0;
		virtual int UnInit() = 0;
		virtual int Start() = 0;
		virtual int Stop() = 0;
		virtual int FillVideo(const void* data, int width, int height, PIX_FMT pix_format) = 0;
		virtual int FillAudio(const void* audioSamples,
							int nb_samples, int sample_rate,
							CHANNEL_LAYOUT chl_layout, AUDIO_FORMAT sample_fmt) = 0;
	protected:
		virtual ~IMediaFileRecorder() {};
	};
}
#endif