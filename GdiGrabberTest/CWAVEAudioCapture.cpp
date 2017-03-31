#include "stdafx.h"
#include "CWAVEAudioCapture.h"

namespace MediaFileRecorder
{
	CWAVEAudioCapture::CWAVEAudioCapture()
		:m_nMicIndex(-1),
		m_nSpeakerIndex(-1),
		m_hWaveIn(NULL)
		//m_hReturnBufferEvent(NULL)
	{
		m_bMicInited = false;
		m_bCapturingMic = false;
		m_bRunning = false;
		InitializeCriticalSection(&m_sectionDataCb);
		//InitializeCriticalSection(&m_sectionReturnBuffer);
	}

	CWAVEAudioCapture::~CWAVEAudioCapture()
	{
		if (m_bCapturingMic)
		{
			StopCaptureMic();
		}
	}

	int CWAVEAudioCapture::RegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb)
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

	int CWAVEAudioCapture::UnRegisterCaptureDataCb(IAudioCaptureDataCb* pDataCb)
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

	int CWAVEAudioCapture::SetMic(int index)
	{
		m_nMicIndex = index;
		return 0;
	}

	int CWAVEAudioCapture::SetSpeaker(int index)
	{
		m_nSpeakerIndex = index;
		return 0;
	}

	int CWAVEAudioCapture::StartCaptureMic()
	{
		if (m_bCapturingMic)
		{
			OutputDebugStringA("Already running \n");
			return -1;
		}

		int ret = InitMic();
		if (ret != 0)
		{
			OutputDebugStringA("InitMic failed \n");
			return -1;
		}

		/*m_hReturnBufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);*/
		//m_RecordThread.swap(std::thread(std::bind(&CWAVEAudioCapture::ReturnWaveBufferThreadProc, this)));

		MMRESULT res = waveInStart(m_hWaveIn);
		if (res != MMSYSERR_NOERROR)
		{
			OutputDebugStringA("waveInStart failed \n");
			return -1;
		}

		StartCaptureMicThread();

		m_bCapturingMic = true;

		return 0;
	}

	int CWAVEAudioCapture::StopCaptureMic()
	{
		if (m_bCapturingMic)
		{
			StopCaptureMicThread();
			waveInStop(m_hWaveIn);
			waveInReset(m_hWaveIn);
			m_bCapturingMic = false;

			UnInitMic();
			return 0;
		}
		return -1;
	}

	int CWAVEAudioCapture::StartCaptureSoundCard()
	{
		return -1;
	}

	int CWAVEAudioCapture::StopCaptureSoundCard()
	{
		return -1;
	}

	int CWAVEAudioCapture::InitMic()
	{
		if (m_bMicInited)
		{
			OutputDebugStringA("Mic already inited \n");
			return -1;
		}

		WAVEFORMATEX waveFormat;
		waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		waveFormat.nChannels = N_REC_CHANNELS;
		waveFormat.nSamplesPerSec = N_REC_SAMPLES_PER_SEC;
		waveFormat.wBitsPerSample = N_REC_BITS_PER_SAMPLE;
		waveFormat.nBlockAlign = waveFormat.nChannels * (waveFormat.wBitsPerSample / 8);
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		waveFormat.cbSize = 0;

		HWAVEIN hWaveIn = NULL;
		uint32_t devID = m_nMicIndex == -1 ? WAVE_MAPPER : (uint32_t)m_nMicIndex;
		MMRESULT res = waveInOpen(NULL, devID, &waveFormat, 0, 0, CALLBACK_NULL | WAVE_FORMAT_QUERY);
		if (res != MMSYSERR_NOERROR)
		{
			OutputDebugStringA("waveInOpen failed for query format \n");
			CleanUpMic();
			return -1;
		}

		res = waveInOpen(&hWaveIn, devID, &waveFormat, NULL, NULL, CALLBACK_NULL);
		if (res != MMSYSERR_NOERROR)
		{
			OutputDebugStringA("Open wave in handle failed \n");
			CleanUpMic();
			return -1;
		}

		if (N_REC_BITS_PER_SAMPLE == 16)
			m_MicAudioInfo.audio_format = AUDIO_FORMAT_16BIT;
		else if (N_REC_BITS_PER_SAMPLE == 8)
			m_MicAudioInfo.audio_format = AUDIO_FORMAT_U8BIT;
		else
		{
			OutputDebugStringA("Unsupported bits per sample \n");
			CleanUpMic();
			return -1;
		}

		m_MicAudioInfo.sample_rate = N_REC_SAMPLES_PER_SEC;
		if (waveFormat.nChannels == 1)
			m_MicAudioInfo.chl_layout = SPEAKERS_MONO;
		else if (waveFormat.nChannels == 2)
			m_MicAudioInfo.chl_layout = SPEAKERS_STEREO;
		else
		{
			OutputDebugStringA("Unsupported channels number \n");
			CleanUpMic();
			return -1;
		}

		m_hWaveIn = hWaveIn;

		for (int n = 0; n < N_BUFFERS_IN; n++)
		{
			uint8_t nBytesPerSample = N_REC_CHANNELS * (N_REC_BITS_PER_SAMPLE / 8);

			m_WaveHeaderIn[n].lpData = (LPSTR)(&m_RecBuffer[n]);
			m_WaveHeaderIn[n].dwBufferLength = nBytesPerSample * REC_BUF_SIZE_IN_SAMPLES;
			m_WaveHeaderIn[n].dwFlags = 0;
			m_WaveHeaderIn[n].dwBytesRecorded = 0;
			m_WaveHeaderIn[n].dwUser = 0;
			m_WaveHeaderIn[n].dwLoops = 1;
			m_WaveHeaderIn[n].lpNext = NULL;
			m_WaveHeaderIn[n].reserved = 0;

			memset(m_RecBuffer[n], 0, nBytesPerSample * REC_BUF_SIZE_IN_SAMPLES);

			res = waveInPrepareHeader(m_hWaveIn, &m_WaveHeaderIn[n], sizeof(WAVEHDR));
			if (res != MMSYSERR_NOERROR)
			{
				OutputDebugStringA("waveInPrepareHeader failed \n");
				CleanUpMic();
				return -1;
			}

			res = waveInAddBuffer(m_hWaveIn, &m_WaveHeaderIn[n], sizeof(WAVEHDR));
			if (res != MMSYSERR_NOERROR)
			{
				OutputDebugStringA("waveInAddBuffer failed \n");
				CleanUpMic();
				return -1;
			}
		}

		m_bMicInited = true;
		return 0;
	}

	int CWAVEAudioCapture::UnInitMic()
	{
		if (m_bMicInited)
		{
			CleanUpMic();
			m_bMicInited = false;
			return 0;
		}
		return -1;
	}


	void CWAVEAudioCapture::CleanUpMic()
	{
		if (m_hWaveIn)
		{
			for (int i = 0; i < N_BUFFERS_IN; i++)
			{
				waveInUnprepareHeader(m_hWaveIn, &m_WaveHeaderIn[i], sizeof(WAVEHDR));
			}
			waveInClose(m_hWaveIn);
			m_hWaveIn = NULL;
		}
	}

	int CWAVEAudioCapture::InitSpeaker()
	{
		return -1;
	}

	int CWAVEAudioCapture::UnInitSpeaker()
	{
		return -1;
	}

	int CWAVEAudioCapture::StartCaptureMicThread()
	{
		if (!m_bRunning)
		{
			m_bRunning = true;
			m_RecordThread.swap(std::thread(std::bind(&CWAVEAudioCapture::MicCaptureThreadProc, this)));
			SetThreadPriority(m_RecordThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
			return 0;
		}
		return -1;
	}

	int CWAVEAudioCapture::StopCaptureMicThread()
	{
		if (m_bRunning)
		{
			m_bRunning = false;
			if (m_RecordThread.joinable())
			{
				m_RecordThread.join();
			}
			return 0;
		}
		return -1;
	}

	void CWAVEAudioCapture::MicCaptureThreadProc()
	{
		HANDLE hWaitableTimer = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER fireTime;
		fireTime.QuadPart = -1;
		SetWaitableTimer(hWaitableTimer, &fireTime, 10, NULL, NULL, FALSE);
		int nBufferIndex = 0;
		int nBytesPerSample = N_REC_CHANNELS * (N_REC_BITS_PER_SAMPLE / 8);
		int nBufferTotalSize = nBytesPerSample * REC_BUF_SIZE_IN_SAMPLES;
		while (true)
		{
			WaitForSingleObject(hWaitableTimer, INFINITE);
			if (!m_bRunning)
				break;

			while (true)
			{
				if (nBufferIndex == N_BUFFERS_IN)
				{
					nBufferIndex = 0;
				}
				int nCapturedSize = m_WaveHeaderIn[nBufferIndex].dwBytesRecorded;
				if (nCapturedSize == nBufferTotalSize)
				{
					int nSamples = nCapturedSize / nBytesPerSample;
					EnterCriticalSection(&m_sectionDataCb);
					for (IAudioCaptureDataCb* pCb : m_vecDataCb)
					{
						pCb->OnCapturedMicData(m_WaveHeaderIn[nBufferIndex].lpData, nSamples, m_MicAudioInfo);
					}
					LeaveCriticalSection(&m_sectionDataCb);

					m_WaveHeaderIn[nBufferIndex].dwBytesRecorded = 0;

					MMRESULT res = waveInUnprepareHeader(m_hWaveIn, &(m_WaveHeaderIn[nBufferIndex]), sizeof(WAVEHDR));
					if (res != MMSYSERR_NOERROR)
					{
						OutputDebugStringA("waveInUnprepareHeader failed \n");
						goto EXIT;
					}

					res = waveInPrepareHeader(m_hWaveIn, &m_WaveHeaderIn[nBufferIndex], sizeof(WAVEHDR));
					if (res != MMSYSERR_NOERROR)
					{
						OutputDebugStringA("waveInPrepareHeader failed \n");
						goto EXIT;
					}
					
					res = waveInAddBuffer(m_hWaveIn, &m_WaveHeaderIn[nBufferIndex], sizeof(WAVEHDR));
					if (res != MMSYSERR_NOERROR)
					{
						OutputDebugStringA("waveInAddBuffer failed \n");
						goto EXIT;
					}

					nBufferIndex++;
				}
				else
				{
					break;
				}
			}
		}
	EXIT:
		CloseHandle(hWaitableTimer);
		return;
	}


	/*void CWAVEAudioCapture::CapturedDataCb(HWAVEIN hwi, UINT uMsg,
		DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
		{
		if (uMsg == WIM_DATA)
		{
		WAVEHDR* pWaveHeader = (WAVEHDR*)dwParam1;
		CWAVEAudioCapture* pThis = (CWAVEAudioCapture*)dwInstance;
		if (pWaveHeader && pThis)
		{
		pThis->OnCapturedData(pWaveHeader);
		}
		}
		}*/


	/*void CWAVEAudioCapture::OnCapturedData(WAVEHDR* pHeader)
	{
	int nBytesPerSample = N_REC_CHANNELS * (N_REC_BITS_PER_SAMPLE / 8);
	int nSamples = pHeader->dwBytesRecorded / nBytesPerSample;
	EnterCriticalSection(&m_sectionDataCb);
	for (IAudioCaptureDataCb* pDataCb : m_vecDataCb)
	{
	pDataCb->OnCapturedMicData(pHeader->lpData, nSamples);
	}
	LeaveCriticalSection(&m_sectionDataCb);

	EnterCriticalSection(&m_sectionReturnBuffer);
	m_vecReturnBuffer.push_back(pHeader);
	LeaveCriticalSection(&m_sectionReturnBuffer);
	SetEvent(m_hReturnBufferEvent);
	}

	void CWAVEAudioCapture::ReturnWaveBufferThreadProc()
	{
	while (m_bRunning)
	{
	DWORD result = WaitForSingleObject(m_hReturnBufferEvent, 100);
	if (result == WAIT_OBJECT_0)
	{
	EnterCriticalSection(&m_sectionReturnBuffer);
	for (WAVEHDR* pHeader : m_vecReturnBuffer)
	{
	pHeader->dwBytesRecorded = 0;
	waveInAddBuffer(m_hWaveIn, pHeader, sizeof(WAVEHDR));
	}
	m_vecReturnBuffer.clear();
	LeaveCriticalSection(&m_sectionReturnBuffer);
	}
	}
	}*/

}