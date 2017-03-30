#ifndef CWAVEAUDIOCAPTURE_H
#define CWAVEAUDIOCAPTURE_H

#include "IAudioCapture.h"
#include <MMSystem.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <atomic>
#include <thread>

namespace MediaFileRecorder
{
	const uint32_t N_REC_BITS_PER_SAMPLE = 16;
	const uint32_t N_REC_SAMPLES_PER_SEC = 48000;
	const uint32_t N_REC_CHANNELS = 1;  // default is mono recording
	// NOTE - CPU load will not be correct for other sizes than 10ms
	const uint32_t REC_BUF_SIZE_IN_SAMPLES = (N_REC_SAMPLES_PER_SEC / 100);
	enum { N_BUFFERS_IN = 200 };

	class CWAVEAudioCapture : public IAudioCapture
	{
	public:
		CWAVEAudioCapture();
		~CWAVEAudioCapture();

		int RegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb) override;
		int UnRegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb) override;
		int SetMic(int index) override;
		int SetSpeaker(int index) override;
		int InitMic() override;
		int UnInitMic() override;
		int InitSpeaker() override;
		int UnInitSpeaker() override;
		int GetMicAudioInfo(AUDIO_INFO& audioInfo) override;
		int GetSoundCardAudioInfo(AUDIO_INFO& audioInfo) override;
		int StartCaptureMic() override;
		int StopCaptureMic() override;
		int StartCaptureSoundCard() override;
		int StopCaptureSoundCard() override;

	private:
		void MicCaptureThreadProc();

		/*static void CapturedDataCb(
			HWAVEIN   hwi,
			UINT      uMsg,
			DWORD_PTR dwInstance,
			DWORD_PTR dwParam1,
			DWORD_PTR dwParam2);
			void OnCapturedData(WAVEHDR* pHeader);
			void ReturnWaveBufferThreadProc();
			std::vector<WAVEHDR*> m_vecReturnBuffer;
			CRITICAL_SECTION m_sectionReturnBuffer;
			HANDLE m_hReturnBufferEvent;*/
	private:
		std::atomic_bool m_bMicInited;
		std::atomic_bool m_bCapturingMic;
		std::atomic_bool m_bRunning;
		std::vector<IAudioCaptureDataCb*> m_vecDataCb;
		CRITICAL_SECTION m_sectionDataCb;
		std::thread m_RecordThread;
		int m_nMicIndex;
		int m_nSpeakerIndex;

		HWAVEIN m_hWaveIn;
		WAVEHDR m_WaveHeaderIn[N_BUFFERS_IN];
		int8_t m_RecBuffer[N_BUFFERS_IN][4 * REC_BUF_SIZE_IN_SAMPLES];
		AUDIO_INFO m_MicAudioInfo;
	};
}

#endif