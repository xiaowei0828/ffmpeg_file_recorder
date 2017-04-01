#ifndef CSCREENAUDIORECORD_H
#define CSCREENAUDIORECORD_H

#include "IScreeAudioRecord.h"
#include "IMediaFileRecorder.h"
#include "IScreenGrabber.h"
#include "IAudioCapture.h"

namespace MediaFileRecorder
{
	class CScreenAudioRecord : public IScreenAudioRecord,
		                       public IScreenGrabberDataCb,
							   public IAudioCaptureDataCb
	{
	public:
		CScreenAudioRecord();
		~CScreenAudioRecord();

		void SetLogCb();
		int SetRecordInfo(const RECORD_INFO& recordInfo) override;
		int StartRecord() override;
		int SuspendRecord() override;
		int ResumeRecord() override;
		int StopRecord() override;

		void OnScreenData(void* data, const VIDEO_INFO& videoInfo) override;
		void OnCapturedData(const void* audioSamples, int nSamples, 
			DEV_TYPE devType, const AUDIO_INFO& audioInfo) override;
	private:
		int CheckRecordInfo();
		void CleanUp();
			;
	private:
		enum RECORD_STATE
		{
			NOT_BEGIN,
			RECORDING,
			SUSPENDED,
		};
		RECORD_STATE m_nRecordState;
		RECORD_INFO m_stRecordInfo;
		IMediaFileRecorder* m_pFileRecorder;
		IScreenGrabber* m_pScreenGrabber;
		IAudioCapture* m_pMicAudioCapturer;
		IAudioCapture* m_pSpeakerAudioCapturer;
	};
}
#endif // !CSCREENAUDIORECORD_H
