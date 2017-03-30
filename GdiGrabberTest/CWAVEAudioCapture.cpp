#include "stdafx.h"
#include "CWAVEAudioCapture.h"

namespace MediaFileRecorder
{
	CWAVEAudioCapture::CWAVEAudioCapture()
		:m_nMicIndex(-1),
		m_nSpeakerIndex(-1),
		m_hWaveIn(NULL)
	{
		m_bMicInited = false;
		m_bCapturingMic = false;
		InitializeCriticalSection(&m_sectionDataCb);
	}

	CWAVEAudioCapture::~CWAVEAudioCapture()
	{
		if (m_bMicInited)
		{
			if (m_bCapturingMic)
			{
				StopCaptureMic();
			}
			UnInitMic();
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
			return -1;
		}

		res = waveInOpen(&hWaveIn, devID, &waveFormat, (DWORD_PTR)&CWAVEAudioCapture::WaveCaptureProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
		if (res != MMSYSERR_NOERROR)
		{
			OutputDebugStringA("Open wave in handle failed \n");
			return -1;
		}

		if (N_REC_BITS_PER_SAMPLE == 16)
			m_MicAudioInfo.audio_format = AUDIO_FORMAT_16BIT;
		else if (N_REC_BITS_PER_SAMPLE == 8)
			m_MicAudioInfo.audio_format = AUDIO_FORMAT_U8BIT;
		else
		{
			OutputDebugStringA("Unsupported bits per sample \n");
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
			return -1;
		}
		m_hWaveIn = hWaveIn;

		m_bMicInited = true;
		return 0;
	}

	int CWAVEAudioCapture::UnInitMic()
	{
		if (m_bMicInited)
		{
			if (m_bCapturingMic)
			{
				StopCaptureMic();
			}
			waveInClose(m_hWaveIn);
			m_hWaveIn = NULL;
			return 0;
		}
		return -1;
	}

	int CWAVEAudioCapture::InitSpeaker()
	{
		return -1;
	}

	int CWAVEAudioCapture::UnInitSpeaker()
	{
		return -1;
	}

	int CWAVEAudioCapture::GetMicAudioInfo(AUDIO_INFO& audioInfo)
	{
		if (m_bMicInited)
		{
			audioInfo = m_MicAudioInfo;
			return 0;
		}
		return -1;
	}

	int CWAVEAudioCapture::GetSoundCardAudioInfo(AUDIO_INFO& audioInfo)
	{
		return -1;
	}

	int CWAVEAudioCapture::StartCaptureMic()
	{
		if (!m_bMicInited)
		{
			OutputDebugStringA("Not inited \n");
			return -1;
		}

		if (m_bCapturingMic)
		{
			OutputDebugStringA("Already running \n");
			return -1;
		}

		MMRESULT res = MMSYSERR_NOERROR;
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
				return -1;
			}

			res = waveInAddBuffer(m_hWaveIn, &m_WaveHeaderIn[n], sizeof(WAVEHDR));
			if (res != MMSYSERR_NOERROR)
			{
				OutputDebugStringA("waveInAddBuffer failed \n");
				return -1;
			}
		}

		res = waveInStart(m_hWaveIn);
		if (res != MMSYSERR_NOERROR)
		{
			OutputDebugStringA("waveInStart failed \n");
			return -1;
		}

		m_bCapturingMic = true;
		return 0;

	}

	int CWAVEAudioCapture::StopCaptureMic()
	{
		if (m_bCapturingMic)
		{
			waveInStop(m_hWaveIn);
			waveInReset(m_hWaveIn);
			for (int i = 0; i < N_BUFFERS_IN; i++)
			{
				waveInUnprepareHeader(m_hWaveIn, &m_WaveHeaderIn[i], sizeof(WAVEHDR));
			}
			m_bCapturingMic = false;
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

	void CALLBACK CWAVEAudioCapture::WaveCaptureProc(HWAVEIN hwi, UINT uMsg, 
		DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
	{
		LPWAVEHDR waveHeader = (LPWAVEHDR)dwParam1;
		if (WIM_DATA == uMsg)
		{
			CWAVEAudioCapture* pThis = (CWAVEAudioCapture*)dwInstance;
			if (pThis)
			{
				pThis->OnCapturedData(waveHeader->lpData, waveHeader->dwBytesRecorded);
			}
		}
	}

	void CWAVEAudioCapture::OnCapturedData(void* data, int nCapturedSize)
	{
		uint32_t nBytesPerSample = N_REC_CHANNELS * (N_REC_BITS_PER_SAMPLE / 8);
		uint32_t nSamplesRecorded = nCapturedSize / nBytesPerSample;

		EnterCriticalSection(&m_sectionDataCb);
		for (auto iter : m_vecDataCb)
		{
			iter->OnCapturedMicData(data, nSamplesRecorded);
		}
		LeaveCriticalSection(&m_sectionDataCb);
	}
}