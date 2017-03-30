#ifndef IMEDIAFILERCORDER_H
#define IMEDIAFILERCORDER_H

#include "MediaRecordTypeDef.h"

namespace MediaFileRecorder
{
	class IMediaFileRecorder
	{
	public:
		virtual int Init(const RECORD_INFO& record_info) = 0;
		virtual int UnInit() = 0;
		virtual int Start() = 0;
		virtual int Stop() = 0;
		virtual int FillVideo(const void* data) = 0;
		virtual int FillMicAudio(const void* audioSamples, int nb_samples) = 0;
		virtual int FillSpeakerAudio(const void* audioSamples, int nb_samples) = 0;

		virtual ~IMediaFileRecorder() {};
	};

	IMediaFileRecorder* CreateMediaFileRecorder();
	void DestroyMediaFileRecorder(IMediaFileRecorder* pMediaFileRecorder);
}
#endif