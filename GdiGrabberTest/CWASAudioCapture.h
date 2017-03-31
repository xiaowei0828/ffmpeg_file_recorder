#ifndef CWASAUDIOCAPTURE_H
#define CWASAUDIOCAPTURE_H

#include "IAudioCapture.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <vector>
#include <atomic>
#include <thread>

namespace MediaFileRecorder
{
	class CWASAudioCapture : public IAudioCapture
	{
	public:
		CWASAudioCapture();
		~CWASAudioCapture();

		int RegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb) override;
		int UnRegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb) override;
		int SetMic(int index) override;
		int SetSpeaker(int index) override;
		int StartCaptureMic() override;
		int StopCaptureMic() override;
		int StartCaptureSoundCard() override;
		int StopCaptureSoundCard() override;

	private:
		int InitMic();
		int UnInitMic();
		void CleanUpMic();
		int InitSpeaker();
		int UnInitSpeaker();
		void CleanUpSpeaker();
		int InitRender();
		int InitFormat(const WAVEFORMATEX* pWfex, AUDIO_INFO& audioInfo);
		CHANNEL_LAYOUT CovertChannelLayout(DWORD dwLayout, int nChannels);

		void CaptureMicThreadProc();
		void CaptureSpeakerThreadProc();
	private:
		std::atomic_bool m_bMicInited;
		std::atomic_bool m_bSpeakerInited;

		std::atomic_bool m_bCapturingMic;
		std::atomic_bool m_bCapturingSpeaker;

		std::vector<IAudioCaptureDataCb*> m_vecDataCb;
		CRITICAL_SECTION m_sectionDataCb;

		int m_nMicIndex;
		int m_nSpeakerIndex;

		CComPtr<IMMDevice> m_pMicDev;
		CComPtr<IAudioClient> m_pMicAudioClient;
		CComPtr<IAudioCaptureClient> m_pMicCaptureClient;

		CComPtr<IAudioClient> m_pSpeakerAudioClient;
		CComPtr<IMMDevice> m_pSpeakerDev;
		CComPtr<IAudioCaptureClient> m_pSpeakerCaptureClient;

		CComPtr<IAudioRenderClient> m_pRenderClient;

		HANDLE m_hMicCaptureStopEvent;
		HANDLE m_hMicCaptureReadyEvent;
		HANDLE m_hSpeakerCaptureSopEvent;

		AUDIO_INFO m_MicAudioInfo;
		AUDIO_INFO m_SpeakerAudioInfo;

		std::thread m_CaptureMicThread;
		std::thread m_CaptureSpeakerThread;
	};
}
#endif