
#include "stdafx.h"
#include "CWASAudioCapture.h"

#define KSAUDIO_SPEAKER_4POINT1 (KSAUDIO_SPEAKER_QUAD|SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_2POINT1 (KSAUDIO_SPEAKER_STEREO|SPEAKER_LOW_FREQUENCY)
#define REFERENCE_TIME_VAL 5 * 10000000

namespace MediaFileRecorder
{
	CWASAudioCapture::CWASAudioCapture()
	{
		m_bMicInited = false;
		m_bSpeakerInited = false;
		m_bCapturingMic = false;
		m_bCapturingSpeaker = false;

		InitializeCriticalSection(&m_sectionDataCb);
		m_hMicCaptureStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hMicCaptureReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hSpeakerCaptureSopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	CWASAudioCapture::~CWASAudioCapture()
	{
		if (m_hMicCaptureStopEvent)
		{
			CloseHandle(m_hMicCaptureStopEvent);
			m_hMicCaptureStopEvent = NULL;
		}
		if (m_hMicCaptureReadyEvent)
		{
			CloseHandle(m_hMicCaptureReadyEvent);
			m_hMicCaptureReadyEvent = NULL;
		}
		if (m_hSpeakerCaptureSopEvent)
		{
			CloseHandle(m_hSpeakerCaptureSopEvent);
			m_hSpeakerCaptureSopEvent = NULL;
		}
	}

	int CWASAudioCapture::RegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb)
	{
		if (std::find(m_vecDataCb.begin(), m_vecDataCb.end(), pDataCb) ==
			m_vecDataCb.end())
		{
			EnterCriticalSection(&m_sectionDataCb);
			m_vecDataCb.push_back(pDataCb);
			LeaveCriticalSection(&m_sectionDataCb);
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::UnRegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb)
	{
		auto iter = std::find(m_vecDataCb.begin(), m_vecDataCb.end(), pDataCb);
		if (iter != m_vecDataCb.end())
		{
			EnterCriticalSection(&m_sectionDataCb);
			m_vecDataCb.erase(iter);
			LeaveCriticalSection(&m_sectionDataCb);
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::SetMic(const char* strUtf8MicID)
	{
		m_strUtf8MicID = strUtf8MicID;
		return 0;
	}

	int CWASAudioCapture::SetSpeaker(const char* strUtf8SpeakerID)
	{
		m_strUtf8SpeakerID = strUtf8SpeakerID;
		return 0;
	}

	int CWASAudioCapture::InitMic()
	{
		if (m_bMicInited)
		{
			OutputDebugStringA("Mic: Alread inited \n");
			return -1;
		}

		CComPtr<IMMDeviceEnumerator> enumerator;
		HRESULT res;

		res = enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator),
			NULL, CLSCTX_ALL);
		if (FAILED(res))
		{
			OutputDebugStringA("Create IMMDeviceEnumerator failed \n");
			return -1;
		}

		if (m_strUtf8MicID.empty())
		{
			res = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &m_pMicDev);
		}
		else
		{
			int length = MultiByteToWideChar(CP_UTF8, NULL, m_strUtf8MicID.c_str(),
				m_strUtf8MicID.length(), NULL, 0);
			wchar_t* strU16DevID = new wchar_t[length + 1];
			MultiByteToWideChar(CP_UTF8, NULL, m_strUtf8MicID.c_str(),
				m_strUtf8MicID.length(), strU16DevID, length + 1);
			res = enumerator->GetDevice(strU16DevID, &m_pMicDev);
			delete strU16DevID;
		}
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: Get Mic IMMdevice failed \n");
			return -1;
		}

		res = m_pMicDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
			NULL, (void**)&m_pMicAudioClient);
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: Activate audio client failed \n");
			return -1;
		}

		WAVEFORMATEX* pWfex;
		res = m_pMicAudioClient->GetMixFormat(&pWfex);
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: GetMixFormat failed \n");
			return -1;
		}

		InitFormat(pWfex, m_MicAudioInfo);

		res = m_pMicAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			REFERENCE_TIME_VAL, 0, pWfex, NULL);
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: Audio client initialize failed \n");
			return -1;
		}

		res = m_pMicAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pMicCaptureClient);
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: get capture service failed \n");
			return -1;
		}

		res = m_pMicAudioClient->SetEventHandle(m_hMicCaptureReadyEvent);
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: SetEventHandle failed \n");
			return -1;
		}

		m_bMicInited = true;

		return 0;
	}

	int CWASAudioCapture::UnInitMic()
	{
		if (m_bMicInited)
		{
			if (m_bCapturingMic)
			{
				StopCaptureMic();
			}
			m_pMicCaptureClient.Release();
			m_pMicAudioClient.Release();
			m_pMicDev.Release();
			m_bMicInited = false;
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::InitSpeaker()
	{
		if (m_bSpeakerInited)
		{
			OutputDebugStringA("Speaker: Already inited");
			return -1;
		}

		CComPtr<IMMDeviceEnumerator> enumerator;
		HRESULT res;

		res = enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator),
			NULL, CLSCTX_ALL);
		if (FAILED(res))
		{
			OutputDebugStringA("Create IMMDeviceEnumerator failed \n");
			return -1;
		}

		if (m_strUtf8SpeakerID.empty())
		{
			res = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pSpeakerDev);
		}
		else
		{
			int length = MultiByteToWideChar(CP_UTF8, NULL, m_strUtf8SpeakerID.c_str(),
				m_strUtf8SpeakerID.length(), NULL, 0);
			wchar_t* strU16DevID = new wchar_t[length + 1];
			MultiByteToWideChar(CP_UTF8, NULL, m_strUtf8SpeakerID.c_str(),
				m_strUtf8SpeakerID.length(), strU16DevID, length + 1);
			res = enumerator->GetDevice(strU16DevID, &m_pSpeakerDev);
			delete strU16DevID;
		}
		if (FAILED(res))
		{
			OutputDebugStringA("Get Speaker IMMdevice failed \n");
			return -1;
		}

		res = m_pSpeakerDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
			NULL, (void**)&m_pSpeakerAudioClient);
		if (FAILED(res))
		{
			OutputDebugStringA("Speaker: Activate audio client failed \n");
			return -1;
		}

		WAVEFORMATEX* pWfex;
		res = m_pSpeakerAudioClient->GetMixFormat(&pWfex);
		if (FAILED(res))
		{
			OutputDebugStringA("Speaker: GetMixFormat failed \n");
			return -1;
		}

		InitFormat(pWfex, m_SpeakerAudioInfo);

		res = m_pSpeakerAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
			REFERENCE_TIME_VAL, 0, pWfex, NULL);
		if (FAILED(res))
		{
			OutputDebugStringA("Speaker: Audio client initialize failed \n");
			return -1;
		}

		InitRender();

		res = m_pSpeakerAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pSpeakerCaptureClient);
		if (FAILED(res))
		{
			OutputDebugStringA("Speaker: get capture service failed \n");
			return -1;
		}

		m_bSpeakerInited = true;

		return 0;
	}

	int CWASAudioCapture::UnInitSpeaker()
	{
		if (m_bSpeakerInited)
		{
			if (m_bCapturingSpeaker)
			{
				StopCaptureSoundCard();
			}
			m_pSpeakerCaptureClient.Release();
			m_pSpeakerAudioClient.Release();
			m_pSpeakerDev.Release();
			m_bSpeakerInited = false;
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::GetMicAudioInfo(AUDIO_INFO& audioInfo)
	{
		if (m_bMicInited)
		{
			audioInfo = m_MicAudioInfo;
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::GetSoundCardAudioInfo(AUDIO_INFO& audioInfo)
	{
		if (m_bSpeakerInited)
		{
			audioInfo = m_SpeakerAudioInfo;
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::StartCaptureMic()
	{
		if (!m_bMicInited)
		{
			OutputDebugStringA("Mic: not inited \n");
			return -1;
		}

		if (m_bCapturingMic)
		{
			OutputDebugStringA("Mic: capture already started \n");
			return -1;
		}

		HRESULT res = m_pMicAudioClient->Start();
		if (FAILED(res))
		{
			OutputDebugStringA("Mic: start capture failed \n");
			return -1;
		}

		m_CaptureMicThread.swap(std::thread(std::bind(&CWASAudioCapture::CaptureMicThreadProc, this)));
		SetThreadPriority(m_CaptureMicThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
		m_bCapturingMic = true;

		return 0;
	}

	int CWASAudioCapture::StopCaptureMic()
	{
		if (!m_bCapturingMic)
		{
			OutputDebugStringA("Mic: capture hasn't been started");
			return -1;
		}

		SetEvent(m_hMicCaptureStopEvent);
		if (m_CaptureMicThread.joinable())
		{
			m_CaptureMicThread.join();
		}

		m_pMicAudioClient->Stop();
		m_bCapturingMic = false;

		return 0;
	}

	int CWASAudioCapture::StartCaptureSoundCard()
	{
		if (!m_bSpeakerInited)
		{
			OutputDebugStringA("Speaker: not inited \n");
			return -1;
		}

		if (m_bCapturingSpeaker)
		{
			OutputDebugStringA("Speaker: already captured \n");
			return -1;
		}

		HRESULT res = m_pSpeakerAudioClient->Start();
		if (FAILED(res))
		{
			OutputDebugStringA("Speaker: start capture failed \n");
			return -1;
		}

		m_CaptureSpeakerThread.swap(std::thread(std::bind(&CWASAudioCapture::CaptureSpeakerThreadProc, this)));
		SetThreadPriority(m_CaptureSpeakerThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
		
		m_bCapturingSpeaker = true;

		return 0;
	}

	int CWASAudioCapture::StopCaptureSoundCard()
	{
		if (!m_bCapturingSpeaker)
		{
			OutputDebugStringA("Speaker: capture hasn't been started");
			return -1;
		}

		SetEvent(m_hSpeakerCaptureSopEvent);
		if (m_CaptureSpeakerThread.joinable())
		{
			m_CaptureSpeakerThread.join();
		}

		m_pSpeakerAudioClient->Stop();

		m_bCapturingSpeaker = false;
		return 0;
	}

	int CWASAudioCapture::InitFormat(const WAVEFORMATEX* pWfex, AUDIO_INFO& audioInfo)
	{
		DWORD layout = 0;
		if (pWfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)pWfex;
			layout = ext->dwChannelMask;
		}
		CHANNEL_LAYOUT chl_layout = CovertChannelLayout(layout, pWfex->nChannels);
		audioInfo.audio_format = AUDIO_FORMAT_FLOAT;
		audioInfo.chl_layout = chl_layout;
		audioInfo.sample_rate = pWfex->nSamplesPerSec;
		return 0;
	}

	CHANNEL_LAYOUT CWASAudioCapture::CovertChannelLayout(DWORD dwLayout, int nChannels)
	{
		switch (dwLayout) {
		case KSAUDIO_SPEAKER_QUAD:             return SPEAKERS_QUAD;
		case KSAUDIO_SPEAKER_2POINT1:          return SPEAKERS_2POINT1;
		case KSAUDIO_SPEAKER_4POINT1:          return SPEAKERS_4POINT1;
		case KSAUDIO_SPEAKER_SURROUND:         return SPEAKERS_SURROUND;
		case KSAUDIO_SPEAKER_5POINT1:          return SPEAKERS_5POINT1;
		case KSAUDIO_SPEAKER_5POINT1_SURROUND: return SPEAKERS_5POINT1_SURROUND;
		case KSAUDIO_SPEAKER_7POINT1:          return SPEAKERS_7POINT1;
		case KSAUDIO_SPEAKER_7POINT1_SURROUND: return SPEAKERS_7POINT1_SURROUND;
		}

		return (CHANNEL_LAYOUT)nChannels;
	}

	void CWASAudioCapture::CaptureMicThreadProc()
	{
		HANDLE waitArray[2] = { m_hMicCaptureStopEvent, m_hMicCaptureReadyEvent };
		while (true)
		{
			DWORD result = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
			if (result == WAIT_OBJECT_0)
			{
				OutputDebugStringA("Mic Thread: receive stop signal, stop the thread \n");
				break;
			}
			else if (result == WAIT_OBJECT_0 + 1)
			{
				HRESULT res;
				LPBYTE  buffer;
				UINT32  frames;
				DWORD   flags;
				UINT64  pos, ts;
				UINT    captureSize = 0;
				while (true)
				{
					res = m_pMicCaptureClient->GetNextPacketSize(&captureSize);
					if (FAILED(res))
					{
						OutputDebugStringA("Mic Thread: some exception occurs during get next pakect size, thread exit \n");
						return;
					}
					if (!captureSize)
						break;

					res = m_pMicCaptureClient->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
					if (FAILED(res))
					{
						OutputDebugStringA("Mic Thread: some exception occurs during get buffer, thread exit");
						return;
					}

					if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
					{
						buffer = NULL;
					}

					for (auto iter : m_vecDataCb)
					{
						iter->OnCapturedMicData(buffer, frames);
					}

					m_pMicCaptureClient->ReleaseBuffer(frames);
				}
			}
		}
	}

	void CWASAudioCapture::CaptureSpeakerThreadProc()
	{
		HANDLE stopEvent = m_hSpeakerCaptureSopEvent;
		while (true)
		{
			DWORD result = WaitForSingleObject(stopEvent, 10);
			if (result == WAIT_OBJECT_0)
			{
				OutputDebugStringA("Speaker Thread: receive stop signal, stop the thread \n");
				break;
			}
			else if (result == WAIT_TIMEOUT)
			{
				HRESULT res;
				LPBYTE  buffer;
				UINT32  frames;
				DWORD   flags;
				UINT64  pos, ts;
				UINT    captureSize = 0;
				while (true)
				{
					res = m_pSpeakerCaptureClient->GetNextPacketSize(&captureSize);
					if (FAILED(res))
					{
						OutputDebugStringA("Speaker Thread: some exception occurs during get next pakect size, thread exit \n");
						return;
					}
					if (!captureSize)
						break;

					res = m_pSpeakerCaptureClient->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
					if (FAILED(res))
					{
						OutputDebugStringA("Speaker Thread: some exception occurs during get buffer, thread exit");
						return;
					}

					/*if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
					{
					buffer = NULL;
					}*/

					for (auto iter : m_vecDataCb)
					{
						iter->OnCapturedSoundCardData(buffer, frames);
					}

					m_pSpeakerCaptureClient->ReleaseBuffer(frames);
				}
			}
		}
	}

	int CWASAudioCapture::InitRender()
	{
		WAVEFORMATEX*              wfex;
		HRESULT                    res;
		LPBYTE                     buffer;
		UINT32                     frames;
		CComPtr<IAudioClient>      client;
		CComPtr<IAudioRenderClient> pRenderClient;

		res = m_pSpeakerDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
			nullptr, (void**)&client);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: failed to activate audio client");
			return -1;
		}

		res = client->GetMixFormat(&wfex);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: failed to get mix format");
			return -1;
		}

		res = client->Initialize(
			AUDCLNT_SHAREMODE_SHARED, 0,
			REFERENCE_TIME_VAL, 0, wfex, nullptr);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: failed to initialize audio client");
			return -1;
		}

		/* Silent loopback fix. Prevents audio stream from stopping and */
		/* messing up timestamps and other weird glitches during silence */
		/* by playing a silent sample all over again. */

		res = client->GetBufferSize(&frames);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: audio client get buffer size failed \n");
			return -1;
		}

		res = client->GetService(__uuidof(IAudioRenderClient),
			(void**)&pRenderClient);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: audio client get render service failed");
			return -1;
		}

		res = pRenderClient->GetBuffer(frames, &buffer);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: render client get buffer failed");
			return -1;
		}

		memset(buffer, 0, frames*wfex->nBlockAlign);

		pRenderClient->ReleaseBuffer(frames, 0);
	}

}