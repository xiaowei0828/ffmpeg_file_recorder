#include "stdafx.h"
#include "CWASAudioCapture.h"

#define KSAUDIO_SPEAKER_4POINT1 (KSAUDIO_SPEAKER_QUAD|SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_2POINT1 (KSAUDIO_SPEAKER_STEREO|SPEAKER_LOW_FREQUENCY)
#define REFERENCE_TIME_VAL 5 * 10000000
#define RECONNECT_INTERVAL 3000

namespace MediaFileRecorder
{

	CWASAudioCapture::CWASAudioCapture(DEV_TYPE devType)
		:m_nDevType(devType),
		m_nDevIndex(-1),
		m_hCaptureThread(NULL),
		m_hReconnectThread(NULL)
	{
		m_bInited = false;
		m_bCapturing = false;
		m_bReconnecting = false;
		InitializeCriticalSection(&m_sectionDataCb);
		m_hCaptureReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hCaptureStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	CWASAudioCapture::~CWASAudioCapture()
	{
		if (m_bCapturing)
		{
			StopCapture();
		}
		if (m_hCaptureReadyEvent)
		{
			CloseHandle(m_hCaptureReadyEvent);
			m_hCaptureReadyEvent = NULL;
		}
		if (m_hCaptureStopEvent)
		{
			CloseHandle(m_hCaptureStopEvent);
			m_hCaptureStopEvent = NULL;
		}
		if (m_hCaptureThread)
		{
			CloseHandle(m_hCaptureThread);
			m_hCaptureThread = NULL;
		}
		if (m_hReconnectThread)
		{
			CloseHandle(m_hReconnectThread);
			m_hReconnectThread = NULL;
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

	int CWASAudioCapture::SetDev(int nIndex)
	{
		m_nDevIndex = nIndex;
		return 0;
	}

	int CWASAudioCapture::StartCapture()
	{
		if (m_bCapturing)
		{
			OutputDebugStringA("Capture already started \n");
			return -1;
		}

		int ret = InitCapture();
		if (ret != 0)
		{
			OutputDebugStringA("InitCapture failed \n");
			return -1;
		}

		HRESULT res = m_pAudioClient->Start();
		if (FAILED(res))
		{
			OutputDebugStringA("Start capture failed \n");
			UnInitCapture();
			return -1;
		}

		m_hCaptureThread = ::CreateThread(NULL, 0, &CWASAudioCapture::CaptureThread,
			(LPVOID)this, 0, NULL);
		
		m_bCapturing = true;

		return 0;
	}

	int CWASAudioCapture::StopCapture()
	{
		if (m_bReconnecting)
		{
			SetEvent(m_hCaptureStopEvent);
			WaitForSingleObject(m_hReconnectThread, INFINITE);
			ResetEvent(m_hCaptureStopEvent);
			return 0;
		}

		if (!m_bCapturing)
		{
			OutputDebugStringA("Capture hasn't been started");
			return -1;
		}

		SetEvent(m_hCaptureStopEvent);
		WaitForSingleObject(m_hCaptureThread, INFINITE);
		ResetEvent(m_hCaptureStopEvent);

		m_pAudioClient->Stop();
		m_bCapturing = false;

		UnInitCapture();

		return 0;
	}

	int CWASAudioCapture::InitCapture()
	{
		if (m_bInited)
		{
			OutputDebugStringA("Already inited \n");
			return -1;
		}

		CoInitialize(NULL);

		CComPtr<IMMDeviceEnumerator> enumerator;
		HRESULT res;

		res = enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator),
 			NULL, CLSCTX_ALL);
		if (FAILED(res))
		{
			OutputDebugStringA("Create IMMDeviceEnumerator failed \n");
			return -1;
		}

		EDataFlow dataFlow;
		if (m_nDevType == MICROPHONE)
			dataFlow = eCapture;
		else
			dataFlow = eRender;

		if (m_nDevIndex == -1)
		{
			res = enumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, &m_pDev);
			if (FAILED(res))
			{
				OutputDebugStringA("Get default capture device failed \n");
				CleanUp();
				return -1;
			}
		}
		else
		{
			CComPtr<IMMDeviceCollection> pDevCollection;
			res = enumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &pDevCollection);
			if (FAILED(res))
			{
				OutputDebugStringA("Get device collection failed \n");
				CleanUp();
				return -1;
			}

			res = pDevCollection->Item(m_nDevIndex, &m_pDev);
			if (FAILED(res))
			{
				OutputDebugStringA("Get device failed \n");
				CleanUp();
				return -1;
			}
		}

		res = m_pDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
			NULL, (void**)&m_pAudioClient);
		if (FAILED(res))
		{
			OutputDebugStringA("Activate audio client failed \n");
			CleanUp();
			return -1;
		}

		WAVEFORMATEX* pWfex;
		res = m_pAudioClient->GetMixFormat(&pWfex);
		if (FAILED(res))
		{
			OutputDebugStringA("GetMixFormat failed \n");
			CleanUp();
			return -1;
		}

		InitFormat(pWfex, m_stAudioInfo);

		DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
		if (m_nDevType == SPEAKER)
			streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

		res = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags,
			REFERENCE_TIME_VAL, 0, pWfex, NULL);
		if (FAILED(res))
		{
			OutputDebugStringA("Audio client initialize failed \n");
			CleanUp();
			return -1;
		}

		if (m_nDevType == SPEAKER)
			InitRender();

		res = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pCaptureClient);
		if (FAILED(res))
		{
			OutputDebugStringA("Get capture service failed \n");
			CleanUp();
			return -1;
		}

		res = m_pAudioClient->SetEventHandle(m_hCaptureReadyEvent);
		if (FAILED(res))
		{
			OutputDebugStringA("SetEventHandle failed \n");
			CleanUp();
			return -1;
		}

		m_bInited = true;

		return 0;
	}

	int CWASAudioCapture::UnInitCapture()
	{
		if (m_bInited)
		{
			CleanUp();
			CoUninitialize();
			m_bInited = false;
			return 0;
		}
		return -1;
	}

	int CWASAudioCapture::InitRender()
	{
		WAVEFORMATEX*              wfex;
		HRESULT                    res;
		LPBYTE                     buffer;
		UINT32                     frames;
		CComPtr<IAudioClient>      client;


		res = m_pDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
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
			(void**)&m_pRenderClient);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: audio client get render service failed");
			return -1;
		}

		res = m_pRenderClient->GetBuffer(frames, &buffer);
		if (FAILED(res))
		{
			OutputDebugStringA("InitRender: render client get buffer failed");
			return -1;
		}

		memset(buffer, 0, frames*wfex->nBlockAlign);

		m_pRenderClient->ReleaseBuffer(frames, 0);

		return 0;
	}

	void CWASAudioCapture::CleanUp()
	{
		m_pDev.Release();
		m_pAudioClient.Release();
		m_pCaptureClient.Release();
		m_pRenderClient.Release();
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

	void CWASAudioCapture::CaptureThreadProc()
	{
		HANDLE waitArray[2] = { m_hCaptureStopEvent, m_hCaptureReadyEvent };
		DWORD dwDuration = m_nDevType == MICROPHONE ? INFINITE : 10;
		bool bReconnect = false;
		while (true)
		{
			DWORD result = WaitForMultipleObjects(2, waitArray, FALSE, dwDuration);
			if (result == WAIT_OBJECT_0)
			{
				OutputDebugStringA("Capture Thread: receive stop signal, stop the thread \n");
				break;
			}
			else if (result == WAIT_OBJECT_0 + 1 || result == WAIT_TIMEOUT)
			{
				if (ProcessCaptureData() != 0)
				{
					bReconnect = true;
					break;
				}
			}
		}
		if (bReconnect)
		{
			m_hReconnectThread = CreateThread(NULL, 0, &CWASAudioCapture::ReconnectThread,
				(LPVOID)this, 0, NULL);
		}
	}

	void CWASAudioCapture::ReconnectThreadProc()
	{
		StopCapture();
		m_bReconnecting = true;
		while (true)
		{
			DWORD result = WaitForSingleObject(m_hCaptureStopEvent, RECONNECT_INTERVAL);
			if (result == WAIT_OBJECT_0)
			{
				OutputDebugStringA("Reconnect thread: Receive stop signal,exit \n");
				break;
			}
			if (StartCapture() == 0)
			{
				m_bReconnecting = false;
				OutputDebugStringA("Reconnect succeed! \n");
				break;
			}
		}
	}

	int CWASAudioCapture::ProcessCaptureData()
	{
		int ret = 0;
		HRESULT res;
		LPBYTE  buffer;
		UINT32  frames;
		DWORD   flags;
		UINT64  pos, ts;
		UINT    captureSize = 0;
		while (true)
		{
			res = m_pCaptureClient->GetNextPacketSize(&captureSize);
			if (FAILED(res))
			{
				OutputDebugStringA("Mic Thread: some exception occurs during get next pakect size, thread exit \n");
				ret = -1;
				break;
			}
			if (!captureSize)
				break;

			res = m_pCaptureClient->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
			if (FAILED(res))
			{
				OutputDebugStringA("Mic Thread: some exception occurs during get buffer, thread exit");
				ret = -1;
				break;
			}

			EnterCriticalSection(&m_sectionDataCb);
			for (auto iter : m_vecDataCb)
			{
				iter->OnCapturedData(buffer, frames, m_nDevType, m_stAudioInfo);
			}
			LeaveCriticalSection(&m_sectionDataCb);
			m_pCaptureClient->ReleaseBuffer(frames);
		}
		return ret;
	}

	DWORD WINAPI CWASAudioCapture::ReconnectThread(LPVOID param)
	{
		CWASAudioCapture* pThis = (CWASAudioCapture*)param;
		if (pThis)
		{
			pThis->ReconnectThreadProc();
		}
		return 0;
	}

	DWORD WINAPI CWASAudioCapture::CaptureThread(LPVOID param)
	{
		CWASAudioCapture* pThis = (CWASAudioCapture*)param;
		if (pThis)
		{
			pThis->CaptureThreadProc();
		}
		return 0;
	}

}