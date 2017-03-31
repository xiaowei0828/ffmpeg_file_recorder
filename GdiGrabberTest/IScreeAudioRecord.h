#ifndef ISCREENAUDIORECORD_H
#define ISCREENAUDIORECORD_H

#include "MediaRecordTypeDef.h"

namespace MediaFileRecorder
{
	class IScreenAudioRecord
	{
	public:
		virtual int SetRecordInfo(const RECORD_INFO& recordInfo) = 0;
		virtual int StartRecord() = 0;
		virtual int SuspendRecord() = 0;
		virtual int ResumeRecord() = 0;
		virtual int StopRecord() = 0;

		virtual ~IScreenAudioRecord(){}
	};

	IScreenAudioRecord* CreateScreenAudioRecorder();
	void DestroyScreenAudioRecorder(IScreenAudioRecord* pRecorder);
}

#endif