#include "stdafx.h"
#include "CScreenAudioRecord.h"
namespace MediaFileRecorder
{


	CScreenAudioRecord::CScreenAudioRecord()
		:m_nRecordState(NOT_BEGIN)
	{
		m_pFileRecorder = CreateMediaFileRecorder();
		m_pScreenGrabber = CreateScreenGrabber();
		m_pAudioCapturer = CreateAudioCapture();

		m_pScreenGrabber->RegisterDataCb(this);
		m_pAudioCapturer->RegisterCaptureDataCb(this);
	}

	CScreenAudioRecord::~CScreenAudioRecord()
	{
		if (m_nRecordState != NOT_BEGIN)
		{
			CleanUp();
		}
		m_pScreenGrabber->UnRegisterDataCb(this);
		m_pScreenGrabber->UnRegisterDataCb(this);
		DestroyMediaFileRecorder(m_pFileRecorder);
		DestroyScreenGrabber(m_pScreenGrabber);
		DestroyAudioCatpure(m_pAudioCapturer);
	}

	int CScreenAudioRecord::SetRecordInfo(const RECORD_INFO& recordInfo)
	{
		if (m_nRecordState != NOT_BEGIN)
		{
			OutputDebugStringA("State not right \n");
			return -1;
		}

		m_stRecordInfo = recordInfo;

		return 0;
	}

	int CScreenAudioRecord::CheckRecordInfo()
	{
		const char* fileName = m_stRecordInfo.file_name;
		if (strlen(fileName) <= 4 || strcmp(fileName + strlen(fileName) - 4, ".mp4") != 0)
		{
			OutputDebugStringA("File name not invalid \n");
			return -1;
		}

		if (m_stRecordInfo.is_record_video)
		{
			const RECT& captureRect = m_stRecordInfo.video_capture_rect;
			if (captureRect.Width() <= 0 || captureRect.Height() <= 0)
			{
				OutputDebugStringA("Capture rect not right \n");
				return -1;
			}

			if (m_stRecordInfo.video_dst_width <= 0 || m_stRecordInfo.video_dst_height <= 0)
			{
				OutputDebugStringA("Target width or height not right \n");
				return -1;
			}

			if (m_stRecordInfo.video_frame_rate <= 0)
			{
				OutputDebugStringA("Capture framerate not right \n");
				return -1;
			}
		}
		return 0;

	}

	int CScreenAudioRecord::StartRecord()
	{
		int ret = 0;
		if (m_nRecordState != NOT_BEGIN)
		{
			OutputDebugStringA("State not right");
			ret |= STATE_NOT_RIGHT;
			return ret;
		}

		if (CheckRecordInfo() != 0)
		{
			OutputDebugStringA("Parameter invalid \n");
			ret |= PARAMETER_INVALID;
			return ret;
		}

		if (m_stRecordInfo.is_record_mic)
		{
			if (m_pAudioCapturer->StartCaptureMic() != 0)
			{
				OutputDebugStringA("Start mic capture failed \n");
				ret |= START_MIC_CAPTURE_FAILED;
			}
		}

		if (m_stRecordInfo.is_record_speaker)
		{
			if (m_pAudioCapturer->StartCaptureSoundCard())
			{
				OutputDebugStringA("Start speaker capture failed \n");
				ret |= START_SPEAKER_CAPTURE_FAILED;
			}
		}

		if (m_stRecordInfo.is_record_video)
		{
			m_pScreenGrabber->SetGrabFrameRate(m_stRecordInfo.video_frame_rate);
			m_pScreenGrabber->SetGrabRect(m_stRecordInfo.video_capture_rect);

			if (m_pScreenGrabber->StartGrab() != 0)
			{
				OutputDebugStringA("Start screen capture failed \n");
				ret |= START_SCRREEN_CAPTURE_FAILED;
			}
		}

		if (m_pFileRecorder->Init(m_stRecordInfo) != 0)
		{
			OutputDebugStringA("Init media file recorder failed \n");
			ret |= INIT_MEDIA_FILE_RECORDER_FAILED;
			CleanUp();
			return ret;
		}
		
		if (m_pFileRecorder->Start() != 0)
		{
			OutputDebugStringA("Start media file recorder failed \n");
			ret |= START_MEDIA_FILE_RECORDER_FAILED;
			CleanUp();
			return ret;
		}

		m_nRecordState = RECORDING;

		return ret;
	}

	int CScreenAudioRecord::SuspendRecord()
	{
		if (m_nRecordState != RECORDING)
		{
			OutputDebugStringA("State not right \n");
			return -1;
		}
		m_nRecordState = SUSPENDED;
		return 0;
	}

	int CScreenAudioRecord::ResumeRecord()
	{
		if (m_nRecordState != SUSPENDED)
		{
			OutputDebugStringA("State not right \n");
			return -1;
		}
		m_nRecordState = RECORDING;
		return 0;
	}

	int CScreenAudioRecord::StopRecord()
	{
		if (m_nRecordState == NOT_BEGIN)
		{
			OutputDebugStringA("State not right \n");
			return -1;
		}

		CleanUp();
		m_nRecordState = NOT_BEGIN;
		return 0;
	}

	void CScreenAudioRecord::OnScreenData(void* data, const VIDEO_INFO& videoInfo)
	{
		if (m_nRecordState == RECORDING)
		{
			m_pFileRecorder->FillVideo(data, videoInfo);
		}
	}

	void CScreenAudioRecord::OnCapturedMicData(const void* audioSamples, int nSamples, 
		const AUDIO_INFO& audioInfo)
	{
		if (m_nRecordState == RECORDING)
		{
			m_pFileRecorder->FillMicAudio(audioSamples, nSamples, audioInfo);
		}
	}

	void CScreenAudioRecord::OnCapturedSoundCardData(const void* audioSamples, int nSamples, 
		const AUDIO_INFO& audioInfo)
	{
		if (m_nRecordState == RECORDING)
		{
			m_pFileRecorder->FillSpeakerAudio(audioSamples, nSamples, audioInfo);
		}
	}

	void CScreenAudioRecord::CleanUp()
	{
		m_pAudioCapturer->StopCaptureSoundCard();
		m_pAudioCapturer->StopCaptureMic();
		m_pScreenGrabber->StopGrab();
		m_pFileRecorder->Stop();
		m_pFileRecorder->UnInit();
	}

	IScreenAudioRecord* CreateScreenAudioRecorder()
	{
		IScreenAudioRecord* pRecorder = new CScreenAudioRecord();
		return pRecorder;
	}

	void DestroyScreenAudioRecorder(IScreenAudioRecord* pRecorder)
	{
		delete pRecorder;
	}
}