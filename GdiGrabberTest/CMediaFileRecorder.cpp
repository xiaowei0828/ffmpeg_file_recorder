#include "stdafx.h"
#include "CMediaFileRecorder.h"
#include <windows.h>
#include <timeapi.h>

void av_log_cb(void* data, int level, const char* msg, va_list args)
{
	char log[1024] = { 0 };
	vsprintf_s(log, msg, args);
	OutputDebugStringA(log);
}

namespace MediaFileRecorder
{
	

	CMediaFileRecorder::CMediaFileRecorder()
		:m_bInited(false),
		m_pFormatCtx(NULL),
		m_pVideoCodecCtx(NULL),
		m_pAudioCodecCtx(NULL),
		m_pOutVideoFrame(NULL),
		m_pOutPicBuffer(NULL),
		m_pInVideoFrame(NULL),
		m_pInPicBuffer(NULL),
		m_nPicSize(0),
		m_pVideoFifoBuffer(NULL),
		m_nVideoFrameIndex(0),
		m_nAudioFrameIndex(0),
		m_pVideoConvertCtx(NULL),
		m_nStartTime(0),
		m_nDuration(0),
		m_nVideoWriteFrames(0),
		m_nVideoDiscardFrames(0),
		m_pMicRecorder(new CAudioRecordBuffer()),
		m_pSpeakerRecorder(new CAudioRecordBuffer()),
		m_pVideoPacket(NULL),
		m_pAudioPacket(NULL)
	{
		m_bRun = false;
		av_log_set_callback(av_log_cb);
	}

	CMediaFileRecorder::~CMediaFileRecorder()
	{
		if (m_bInited)
		{
			UnInit();
		}
	}

	int CMediaFileRecorder::Init(const RECORD_INFO& record_info)
	{
		if (m_bInited)
		{
			OutputDebugStringA("Inited already!\n");
			return -1;
		}

		int ret = 0;

		m_stRecordInfo = record_info;

		av_register_all();

		ret = avformat_alloc_output_context2(&m_pFormatCtx, NULL, NULL, m_stRecordInfo.file_name);
		if (ret < 0)
		{
			OutputDebugStringA("avformat_alloc_output_context2\n");
			CleanUp();
			return -1;
		}

		ret = avio_open(&m_pFormatCtx->pb, m_stRecordInfo.file_name, AVIO_FLAG_READ_WRITE);
		if (ret < 0)
		{
			OutputDebugStringA("avio_open failed\n");
			CleanUp();
			return -1;
		}

		
		if (m_stRecordInfo.is_record_video)
		{
			if (InitVideoRecord() != 0)
			{
				OutputDebugStringA("Init video record failed\n");
			}
		}

		if (m_stRecordInfo.is_record_mic || m_stRecordInfo.is_record_speaker)
		{
			if (InitAudioRecord() != 0)
			{
				OutputDebugStringA("Init audio record failed\n");
			}
		}

		InitializeCriticalSection(&m_WriteFileSection);

		//write file header
		avformat_write_header(m_pFormatCtx, NULL);

		m_bInited = true;
		
		return 0;
	}


	int CMediaFileRecorder::InitVideoRecord()
	{
		VIDEO_INFO& video_info = m_stRecordInfo.video_info;

		AVStream* pVideoStream = avformat_new_stream(m_pFormatCtx, NULL);
		if (pVideoStream == NULL)
		{
			OutputDebugStringA("Create new video stream failed\n");
			VideoCleanUp();
			return -1;
		}

		m_pVideoCodecCtx = pVideoStream->codec;
		m_pVideoCodecCtx->codec_id = AV_CODEC_ID_H264;
		m_pVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
		m_pVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
		m_pVideoCodecCtx->width = m_stRecordInfo.video_info.dst_width;
		m_pVideoCodecCtx->height = m_stRecordInfo.video_info.dst_height;

		m_pVideoCodecCtx->time_base.num = 1;
		m_pVideoCodecCtx->time_base.den = m_stRecordInfo.video_info.frame_rate;
		m_pVideoCodecCtx->thread_count = 5;
		//m_pVideoCodecCtx->bit_rate = 1024 * 512 * 8;

		m_pVideoCodecCtx->max_qdiff = 4;
		m_pVideoCodecCtx->qmin = 20;
		m_pVideoCodecCtx->qmax = 30;
		m_pVideoCodecCtx->qcompress = (float)0.6;
		m_pVideoCodecCtx->gop_size = m_stRecordInfo.video_info.frame_rate * 10;

		//m_pVideoCodecCtx->flags &= CODEC_FLAG_QSCALE;
		// Set option
		AVDictionary *param = 0;
		av_dict_set(&param, "preset", "veryfast", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);
		/*av_dict_set(&param, "profile", "main", 0);*/

		AVCodec* pEncoder = avcodec_find_encoder(m_pVideoCodecCtx->codec_id);
		if (!pEncoder)
		{
			OutputDebugStringA("avcodec_findo_encoder failed\n");
			VideoCleanUp();
			return -1;
		}

		if (m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			pVideoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		int ret = avcodec_open2(m_pVideoCodecCtx, pEncoder, &param);
		if (ret < 0)
		{
			OutputDebugStringA("avcodec_open2 failed\n");
			VideoCleanUp();
			return -1;
		}

		m_nPicSize = avpicture_get_size(m_pVideoCodecCtx->pix_fmt, m_pVideoCodecCtx->width, m_pVideoCodecCtx->height);
		m_pOutVideoFrame = av_frame_alloc();
		m_pOutPicBuffer = (uint8_t*)av_malloc(m_nPicSize);
		avpicture_fill((AVPicture*)m_pOutVideoFrame, m_pOutPicBuffer, m_pVideoCodecCtx->pix_fmt,
			m_pVideoCodecCtx->width, m_pVideoCodecCtx->height);

		m_pInVideoFrame = av_frame_alloc();
		m_pInPicBuffer = (uint8_t*)av_malloc(m_nPicSize);
		avpicture_fill((AVPicture*)m_pInVideoFrame, m_pInPicBuffer, m_pVideoCodecCtx->pix_fmt,
			m_pVideoCodecCtx->width, m_pVideoCodecCtx->height);

		m_pVideoPacket = new AVPacket();
		av_new_packet(m_pVideoPacket, m_nPicSize);

		// Init swscontext
		AVPixelFormat av_pix_fmt;
		if (!ConvertToAVPixelFormat(video_info.src_pix_fmt, av_pix_fmt))
		{
			OutputDebugStringA("InitVideoRecord: convert to av pixel format failed \n");
			VideoCleanUp();
			return -1;
		}
		
		m_pVideoConvertCtx = sws_getContext(
			video_info.src_width, video_info.src_height, 
			av_pix_fmt, video_info.dst_width, 
			video_info.dst_height, AV_PIX_FMT_YUV420P,
			NULL, NULL,
			NULL, NULL);
		if (!m_pVideoConvertCtx)
		{
			OutputDebugStringA("InitVideo: get convert context failed \n");
			VideoCleanUp();
			return -1;
		}

		//申请30帧缓存
		m_pVideoFifoBuffer = av_fifo_alloc(30 * m_nPicSize);

		InitializeCriticalSection(&m_VideoSection);

		return 0;	

	}

	void CMediaFileRecorder::UnInitVideoRecord()
	{
		char log[128] = { 0 };
		_snprintf_s(log, 128, "Write video count: %lld, Discard frame count: %lld \n",
			m_nVideoWriteFrames, m_nVideoDiscardFrames);
		OutputDebugStringA(log);

		VideoCleanUp();
	}

	void CMediaFileRecorder::VideoCleanUp()
	{
		if (m_pVideoPacket)
		{
			av_free_packet(m_pVideoPacket);
			delete m_pVideoPacket;
			m_pVideoPacket = NULL;
		}
		
		if (m_pVideoCodecCtx)
		{
			avcodec_close(m_pVideoCodecCtx);
			m_pVideoCodecCtx = NULL;
		}
		if (m_pOutVideoFrame)
		{
			av_free(m_pOutVideoFrame);
			m_pOutVideoFrame = NULL;
		}
		if (m_pOutPicBuffer)
		{
			av_free(m_pOutPicBuffer);
			m_pOutPicBuffer = NULL;
		}
		if (m_pInVideoFrame)
		{
			av_free(m_pInVideoFrame);
			m_pInVideoFrame = NULL;
		}
		if (m_pInPicBuffer)
		{
			av_free(m_pInPicBuffer);
			m_pInPicBuffer = NULL;
		}
		if (m_pVideoFifoBuffer)
		{
			av_fifo_free(m_pVideoFifoBuffer);
			m_pVideoFifoBuffer = NULL;
		}
		if (m_pVideoConvertCtx)
		{
			sws_freeContext(m_pVideoConvertCtx);
			m_pVideoConvertCtx = NULL;
		}
		m_nPicSize = 0;
		m_nVideoFrameIndex = 0;
		m_nVideoWriteFrames = 0;
		m_nVideoDiscardFrames = 0;
	}

	

	int CMediaFileRecorder::InitAudioRecord()
	{
		AVStream* pStream = avformat_new_stream(m_pFormatCtx, NULL);
		if (!pStream)
		{
			OutputDebugStringA("create audio stream failed! \n");
			AudioCleanUp();
			return -1;
		}

		m_pAudioCodecCtx = pStream->codec;
		m_pAudioCodecCtx->codec_id = AV_CODEC_ID_AAC;
		m_pAudioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
		m_pAudioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
		m_pAudioCodecCtx->sample_rate = 44100;
		m_pAudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
		m_pAudioCodecCtx->channels = av_get_channel_layout_nb_channels(m_pAudioCodecCtx->channel_layout);
		//m_pAudioCodecCtx->bit_rate = 128000;

		AVCodec* audio_encoder = avcodec_find_encoder(m_pAudioCodecCtx->codec_id);
		if (!audio_encoder)
		{
			OutputDebugStringA("failed to find audio encoder! \n");
			AudioCleanUp();
			return -1;
		}

		if (m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			pStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		if (avcodec_open2(m_pAudioCodecCtx, audio_encoder, NULL) < 0)
		{
			OutputDebugStringA("failed to open audio codec context");
			AudioCleanUp();
			return -1;
		}

		int nAudioSize = av_samples_get_buffer_size(NULL, m_pAudioCodecCtx->channels,
			m_pAudioCodecCtx->frame_size, m_pAudioCodecCtx->sample_fmt, 1);

		m_pAudioPacket = new AVPacket();
		av_new_packet(m_pAudioPacket, nAudioSize);

		if (m_stRecordInfo.is_record_mic)
		{
			if (m_pMicRecorder->Init(m_pAudioCodecCtx, m_stRecordInfo.mic_audio_info) != 0)
			{
				OutputDebugStringA("failed to init mic recorder \n");
			}
		}

		if (m_stRecordInfo.is_record_speaker)
		{
			if (m_pSpeakerRecorder->Init(m_pAudioCodecCtx, m_stRecordInfo.speaker_audio_info) != 0)
			{
				OutputDebugStringA("failed to init speaker recorder \n");
			}
		}

		return 0;
	}

	void CMediaFileRecorder::UnInitAudioRecord()
	{
		m_pMicRecorder->UnInit();
		m_pSpeakerRecorder->UnInit();

		char log[128] = { 0 };
		_snprintf_s(log, 128, "Write audio frame count: %lld \n",
			m_nAudioFrameIndex);
		OutputDebugStringA(log);
		AudioCleanUp();

		return;
	}

	void CMediaFileRecorder::AudioCleanUp()
	{
		if (m_pAudioPacket)
		{
			av_free_packet(m_pAudioPacket);
			delete m_pAudioPacket;
			m_pAudioPacket = NULL;
		}
		if (m_pAudioCodecCtx)
		{
			avcodec_close(m_pAudioCodecCtx);
			m_pAudioCodecCtx = NULL;
		}

		m_nAudioFrameIndex = 0;
	}


	int CMediaFileRecorder::UnInit()
	{
		if (m_bInited)
		{
			if (m_bRun)
			{
				StopWriteFileThread();
			}
			av_write_trailer(m_pFormatCtx);
			UnInitVideoRecord();
			UnInitAudioRecord();
			CleanUp();
			m_bInited = false;
			return 0;
		}
		return -1;
	}


	void CMediaFileRecorder::CleanUp()
	{
		if (m_pFormatCtx)
		{
			avio_close(m_pFormatCtx->pb);
			avformat_free_context(m_pFormatCtx);
			m_pFormatCtx = NULL;

		}
		m_nStartTime = 0;
		m_nDuration = 0;
	}


	int CMediaFileRecorder::Start()
	{
		if (!m_bInited)
		{
			OutputDebugStringA("Not inited\n");
			return -1;
		}
		m_nStartTime = timeGetTime();
		StartWriteFileThread();
		return 0;
	}

	int CMediaFileRecorder::Stop()
	{
		if (m_bRun)
		{
			StopWriteFileThread();
			m_nDuration += timeGetTime() - m_nStartTime;
			return 0;
		}
		return - 1;
	}

	int CMediaFileRecorder::FillVideo(const void* data)
	{
		int ret = -1;
		if (m_stRecordInfo.is_record_video && m_bInited && m_bRun)
		{
			int64_t begin_time = timeGetTime();
			VIDEO_INFO& video_info = m_stRecordInfo.video_info;

			int64_t nVideoCaptureTime = m_nDuration + (timeGetTime() - m_nStartTime);
			int size = video_info.dst_width * video_info.dst_height;

			if (av_fifo_space(m_pVideoFifoBuffer) >= m_nPicSize + (int)sizeof(int64_t))
			{
				int bytes_per_pix;
				if (video_info.src_pix_fmt == PIX_FMT_ARGB || 
					video_info.src_pix_fmt  == PIX_FMT_BGRA)
					bytes_per_pix = 4;
				else if (video_info.src_pix_fmt == PIX_FMT_BGR24 ||
					video_info.src_pix_fmt == PIX_FMT_RGB24)
					bytes_per_pix = 3;

				uint8_t* srcSlice[3] = { (uint8_t*)data, NULL, NULL };
				int srcStride[3] = { bytes_per_pix * video_info.src_width, 0, 0 };
				sws_scale(m_pVideoConvertCtx, srcSlice, srcStride, 0, video_info.src_height,
					m_pInVideoFrame->data, m_pInVideoFrame->linesize);

				EnterCriticalSection(&m_VideoSection);
				av_fifo_generic_write(m_pVideoFifoBuffer, &nVideoCaptureTime, sizeof(int64_t), NULL);
				av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[0], size, NULL);
				av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[1], size / 4, NULL);
				av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[2], size / 4, NULL);
				LeaveCriticalSection(&m_VideoSection);
				m_nVideoWriteFrames++;

				ret = 0;
			}
			else
			{
				m_nVideoDiscardFrames++;
			}
			int64_t duration = timeGetTime() - begin_time;
			char log[128] = { 0 };
			_snprintf_s(log, 128, "FillVideo spend time: %lld \n", duration);
			OutputDebugStringA(log);
		}
		return ret;
	}


	int CMediaFileRecorder::FillMicAudio(const void* audioSamples, int nb_samples)
	{
		int ret = -1;
		if (m_stRecordInfo.is_record_mic && m_bInited & m_bRun)
		{
			int64_t begin_time = timeGetTime();

			ret = m_pMicRecorder->FillAudio(audioSamples, nb_samples);
			int64_t resample_time = timeGetTime() - begin_time;

			char log[128] = { 0 };
			sprintf_s(log, "Mic FillAudio time: %lld \n", resample_time);
			OutputDebugStringA(log);
		}

		return ret;
	}


	int CMediaFileRecorder::FillSpeakerAudio(const void* audioSamples, int nb_samples)
	{
		int ret = -1;
		if (m_stRecordInfo.is_record_speaker && m_bInited & m_bRun)
		{
			int64_t begin_time = timeGetTime();

			ret = m_pSpeakerRecorder->FillAudio(audioSamples, nb_samples);
			int64_t resample_time = timeGetTime() - begin_time;

			char log[128] = { 0 };
			sprintf_s(log, "Speaker FillAudio time: %lld \n", resample_time);
			OutputDebugStringA(log);
		}

		return ret;
	}


	void CMediaFileRecorder::StartWriteFileThread()
	{
		m_bRun = true;
		if (m_stRecordInfo.is_record_video)
		{
			m_WriteVideoThread.swap(std::thread(std::bind(&CMediaFileRecorder::VideoWriteFileThreadProc, this)));
			SetThreadPriority(m_WriteVideoThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
		}
		if (m_stRecordInfo.is_record_mic ||
			m_stRecordInfo.is_record_speaker)
		{
			m_WriteAudioThread.swap(std::thread(std::bind(&CMediaFileRecorder::AuidoWriteFileThreadProc, this)));
			SetThreadPriority(m_WriteAudioThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
		}

		
	}

	void CMediaFileRecorder::StopWriteFileThread()
	{
		m_bRun = false;
		if (m_WriteVideoThread.joinable())
		{
			m_WriteVideoThread.join();
		}
		if (m_WriteAudioThread.joinable())
		{
			m_WriteAudioThread.join();
		}
	}

	void CMediaFileRecorder::VideoWriteFileThreadProc()
	{
		char log[128] = { 0 };
		int sleep_time = 5;
		while (m_bRun)
		{
			// first check the buffer size
			int space = av_fifo_space(m_pVideoFifoBuffer);
			int used_size = av_fifo_size(m_pVideoFifoBuffer);
			int total_size = space + used_size;
			int ratio = total_size / space;
			if (ratio >= 10)
			{
				// buffer may be insufficient, grow the buffer.
				EnterCriticalSection(&m_VideoSection);
				// grow 1/3
				sprintf_s(log, "total size: %d kb, space: %d kb, used: %d kb, grow: %d kb \n",
					total_size / 1024, space / 1024, used_size / 1024, total_size / (3 * 1024));
				OutputDebugStringA(log);
				int ret = av_fifo_grow(m_pVideoFifoBuffer, total_size / 3);
				if (ret < 0)
					OutputDebugStringA("grow failed \n");
				else
					OutputDebugStringA("grow succeed! \n");

				LeaveCriticalSection(&m_VideoSection);
				sleep_time = 1;
			}
			else if (ratio <= 2)
			{
				// buffer is too big, resize it 
				EnterCriticalSection(&m_VideoSection);
				// shrink 1 / 3
				sprintf_s(log, "total size: %d kb, space: %d kb, used: %d kb, shrink: %d kb \n",
					total_size / 1024, space / 1024, used_size / 1024, total_size / (3 * 1024));
				OutputDebugStringA(log);
				int ret = av_fifo_realloc2(m_pVideoFifoBuffer, total_size * 2 / 3);
				if (ret < 0)
					OutputDebugStringA("shrink failed \n");
				else
					OutputDebugStringA("shrink succeed! \n");

				LeaveCriticalSection(&m_VideoSection);
				sleep_time = 5;
			}

			EncodeAndWriteVideo();

			Sleep(sleep_time);
		}

		// Write the left data to file
		while (av_fifo_size(m_pVideoFifoBuffer) >= m_nPicSize + (int)sizeof(int64_t))
		{
			OutputDebugStringA("Write the left video data to file \n");
			EncodeAndWriteVideo();
		}
	}

	void CMediaFileRecorder::EncodeAndWriteVideo()
	{
		int64_t begin_time = timeGetTime();

		if (av_fifo_size(m_pVideoFifoBuffer) >= m_nPicSize + (int)sizeof(int64_t))
		{
			int64_t nCaptureTime;
			EnterCriticalSection(&m_VideoSection);
			av_fifo_generic_read(m_pVideoFifoBuffer, &nCaptureTime, sizeof(int64_t), NULL);
			av_fifo_generic_read(m_pVideoFifoBuffer, m_pOutPicBuffer, m_nPicSize, NULL);
			LeaveCriticalSection(&m_VideoSection);

			int bGotPicture = 0;

			int64_t encode_start_time = timeGetTime();
			int ret = avcodec_encode_video2(m_pVideoCodecCtx, m_pVideoPacket, m_pOutVideoFrame, &bGotPicture);
			int64_t encode_duration = timeGetTime() - encode_start_time;

			if (ret < 0)
			{
				return;
			}

			if (bGotPicture == 1)
			{
				AVStream* pStream = m_pFormatCtx->streams[0];
				m_pVideoPacket->pts = nCaptureTime *(pStream->time_base.den/pStream->time_base.num) / 1000;
				m_pVideoPacket->dts = m_pVideoPacket->pts;
				//m_VideoPacket.duration = 1;

				EnterCriticalSection(&m_WriteFileSection);
				ret = av_interleaved_write_frame(m_pFormatCtx, m_pVideoPacket);
				LeaveCriticalSection(&m_WriteFileSection);

				char log[128] = { 0 };
				_snprintf_s(log, 128, "encode frame: %lld\n", encode_duration);
				OutputDebugStringA(log);
			}
			m_nVideoFrameIndex++;
		}
		int64_t duration = timeGetTime() - begin_time;

		char log[128] = { 0 };
		sprintf_s(log, "Write video file time: %lld, used size: %d, space: %d \n",
			duration, av_fifo_size(m_pVideoFifoBuffer), av_fifo_space(m_pVideoFifoBuffer));
		OutputDebugStringA(log);

	}

	void CMediaFileRecorder::AuidoWriteFileThreadProc()
	{
		while (m_bRun)
		{
			EncodeAndWriteAudio();
			Sleep(5);
		}
	}

	void CMediaFileRecorder::EncodeAndWriteAudio()
	{
		int64_t begin_time = timeGetTime();
		AVFrame* pDstFrame = NULL;

		if (m_stRecordInfo.is_record_speaker)
		{
			pDstFrame = m_pSpeakerRecorder->GetOneFrame();
			if (!pDstFrame)
				return;
		}

		if (m_stRecordInfo.is_record_mic)
		{
			AVFrame* pMicFrame = m_pMicRecorder->GetOneFrame();
			if (pMicFrame)
			{
				float* left1 = (float*)pDstFrame->data[0];
				float* right1 = (float*)pDstFrame->data[1];
				float* left2 = (float*)pMicFrame->data[0];
				float* right2 = (float*)pMicFrame->data[1];
				for (int i = 0; i < m_pAudioCodecCtx->frame_size; i++)
				{
					MixAudio(left1[i], left1[i], left2[i]);
					MixAudio(right1[i], right1[i], right2[i]);
				}
			}
		}
		int64_t mix_time = timeGetTime() - begin_time;

		if (pDstFrame)
		{
			int got_picture = 0;
			//pSpeakerFrame->pts = m_nAudioFrameIndex * m_pAudioCodecCtx->frame_size;
			if (avcodec_encode_audio2(m_pAudioCodecCtx, m_pAudioPacket, pDstFrame, &got_picture) < 0)
			{
				OutputDebugStringA("avcodec_encode_audio2 failed");
			}
			if (got_picture)
			{
				int64_t pts = (m_nDuration + (timeGetTime() - m_nStartTime)) * m_pAudioCodecCtx->sample_rate / 1000;
				m_pAudioPacket->pts = pts/*m_nAudioFrameIndex * m_pAudioCodecCtx->frame_size*/;
				m_pAudioPacket->dts = pts/*m_nAudioFrameIndex * m_pAudioCodecCtx->frame_size*/;
				m_pAudioPacket->duration = m_pAudioCodecCtx->frame_size;


				//cur_pts_a = m_AudioPacket.pts;

				EnterCriticalSection(&m_WriteFileSection);
				av_interleaved_write_frame(m_pFormatCtx, m_pAudioPacket);
				LeaveCriticalSection(&m_WriteFileSection);
				m_nAudioFrameIndex++;
			}
		}
		int64_t duration = timeGetTime() - begin_time;
		char log[128] = { 0 };
		sprintf_s(log, "Mix audio time: %lld \n", mix_time);
		OutputDebugStringA(log);
	}

	void CMediaFileRecorder::MixAudio(float& dst, float& src1, float& src2)
	{
		if (src1 < 0 && src2 < 0)
		{
			dst = (src1 + src2) - src1 * src2 / (-(pow(2, 32 - 1) - 1));
		}
		else
		{
			dst = (src1 + src2) - src1 * src2 /(pow(2, 32 - 1));
		}
	}


//////////////////////////////////////////////////////////////////////////////
//// Class CAudioRecord

	CAudioRecordBuffer::CAudioRecordBuffer()
		:m_pConvertCtx(NULL),
		m_pFifoBuffer(NULL),
		m_nResampleBufferSize(0),
		m_nSavedAudioSamples(0),
		m_nDiscardAudioSamples(0)
	{
		m_bInited = false;
		memset(m_pResampleBuffer, 0, sizeof(m_pResampleBuffer));
	}

	CAudioRecordBuffer::~CAudioRecordBuffer()
	{

	}

	int CAudioRecordBuffer::Init(const AVCodecContext* pCodecCtx, const AUDIO_INFO& src_audio_info)
	{
		if (m_bInited)
		{
			OutputDebugStringA("AudioRecorder: Already inited \n");
			return -1;
		}

		m_pCodecCtx = pCodecCtx;
		m_stAudioInfo = src_audio_info;

		AVSampleFormat src_av_sample_fmt;
		if (!ConvertToAVSampleFormat(m_stAudioInfo.audio_format, src_av_sample_fmt))
		{
			OutputDebugStringA("FiilAudio: convert to av sample format failed \n");
			CleanUp();
			return -1;
		}

		int64_t src_av_ch_layout;
		if (!ConvertToAVChannelLayOut(m_stAudioInfo.chl_layout, src_av_ch_layout))
		{
			OutputDebugStringA("FillAudio: convert to av channel layout failed \n");
			CleanUp();
			return -1;
		}

		int src_nb_channels = av_get_channel_layout_nb_channels(src_av_sample_fmt);

		m_pConvertCtx = swr_alloc();
		// init audio resample context
		//设置源通道布局  
		av_opt_set_int(m_pConvertCtx, "in_channel_layout", src_av_ch_layout, 0);
		//设置源通道采样率  
		av_opt_set_int(m_pConvertCtx, "in_sample_rate", src_audio_info.sample_rate, 0);
		//设置源通道样本格式  
		av_opt_set_sample_fmt(m_pConvertCtx, "in_sample_fmt", src_av_sample_fmt, 0);

		//目标通道布局  
		av_opt_set_int(m_pConvertCtx, "out_channel_layout", m_pCodecCtx->channel_layout, 0);
		//目标采用率  
		av_opt_set_int(m_pConvertCtx, "out_sample_rate", m_pCodecCtx->sample_rate, 0);
		//目标样本格式  
		av_opt_set_sample_fmt(m_pConvertCtx, "out_sample_fmt", m_pCodecCtx->sample_fmt, 0);

		if (swr_init(m_pConvertCtx) < 0)
		{
			OutputDebugStringA("swr_init failed in FillAudio. \n");
			CleanUp();
			return -1;
		}

		m_pFrame = av_frame_alloc();
		m_pFrame->nb_samples = m_pCodecCtx->frame_size;
		m_pFrame->format = m_pCodecCtx->sample_fmt;

		int nSize = av_samples_get_buffer_size(NULL, m_pCodecCtx->channels,
			m_pCodecCtx->frame_size, m_pCodecCtx->sample_fmt, 1);
		m_pFrameBuffer = (uint8_t *)av_malloc(nSize);
		avcodec_fill_audio_frame(m_pFrame, m_pCodecCtx->channels,
			m_pCodecCtx->sample_fmt, (const uint8_t*)m_pFrameBuffer, nSize, 1);

		m_pFifoBuffer = av_audio_fifo_alloc(pCodecCtx->sample_fmt,
			pCodecCtx->channels, 100 * m_pCodecCtx->frame_size);
		if (!m_pFifoBuffer)
		{
			CleanUp();
			return -1;
		}

		InitializeCriticalSection(&m_BufferSection);

		m_bInited = true;

		return 0;
	}

	int CAudioRecordBuffer::FillAudio(const void* audioSamples, int nb_samples)
	{
		if (m_bInited)
		{
			int ret = -1;

			int dst_nb_samples = (int)av_rescale_rnd(
				swr_get_delay(m_pConvertCtx, m_stAudioInfo.sample_rate) + nb_samples,
				m_pCodecCtx->sample_rate, m_stAudioInfo.sample_rate, AV_ROUND_UP);

			if (dst_nb_samples <= 0)
			{
				OutputDebugStringA("av_rescale_rnd failed. \n");
				return ret;
			}

			if (dst_nb_samples > m_nResampleBufferSize)
			{
				if (m_pResampleBuffer[0])
					av_freep(&m_pResampleBuffer[0]);
				av_samples_alloc(m_pResampleBuffer, NULL, m_pCodecCtx->channels,
					dst_nb_samples, m_pCodecCtx->sample_fmt, 0);
				m_nResampleBufferSize = dst_nb_samples;
			}

			int ret_samples = swr_convert(m_pConvertCtx, m_pResampleBuffer, dst_nb_samples,
				(const uint8_t**)&audioSamples, nb_samples);
			if (ret_samples <= 0)
			{
				OutputDebugStringA("swr_convert failed. \n");
				return ret;
			}

			if (av_audio_fifo_space(m_pFifoBuffer) >= ret_samples)
			{
				EnterCriticalSection(&m_BufferSection);
				av_audio_fifo_write(m_pFifoBuffer, (void **)m_pResampleBuffer, ret_samples);
				LeaveCriticalSection(&m_BufferSection);
				m_nSavedAudioSamples += nb_samples;
				ret = 0;
			}
			else
			{
				m_nDiscardAudioSamples += nb_samples;
			}

			return ret;
		}
		return -1;
	}


	AVFrame* CAudioRecordBuffer::GetOneFrame()
	{
		if (m_bInited)
		{
			if (av_audio_fifo_size(m_pFifoBuffer) >= m_pCodecCtx->frame_size)
			{
				EnterCriticalSection(&m_BufferSection);
				av_audio_fifo_read(m_pFifoBuffer, (void**)m_pFrame->data,
					m_pCodecCtx->frame_size);
				LeaveCriticalSection(&m_BufferSection);
				return m_pFrame;
			}
		}
		return NULL;
	}


	int CAudioRecordBuffer::UnInit()
	{
		if (m_bInited)
		{
			char log[128] = { 0 };
			_snprintf_s(log, 128, "save samples: %lld, discard samples: %lld\n",
				m_nSavedAudioSamples, m_nDiscardAudioSamples);
			OutputDebugStringA(log);
			CleanUp();
			m_bInited = false;
			return 0;
		}
		return -1;
	}

	void CAudioRecordBuffer::CleanUp()
	{
		if (m_pFrame)
		{
			av_free(m_pFrame);
			m_pFrame = NULL;
		}
		if (m_pFrameBuffer)
		{
			av_free(m_pFrameBuffer);
			m_pFrameBuffer = NULL;
		}
		if (m_pConvertCtx)
		{
			swr_free(&m_pConvertCtx);
			m_pConvertCtx = NULL;
		}
		if (m_pFifoBuffer)
		{
			av_audio_fifo_free(m_pFifoBuffer);
			m_pFifoBuffer = NULL;
		}
		if (m_pResampleBuffer[0])
		{
			av_freep(m_pResampleBuffer);
			memset(m_pResampleBuffer, 0, sizeof(m_pResampleBuffer));
		}
		m_nResampleBufferSize = 0;
		m_nSavedAudioSamples = 0;
		m_nDiscardAudioSamples = 0;
	}

	bool ConvertToAVSampleFormat(AUDIO_FORMAT audio_format, AVSampleFormat& av_sample_fmt)
	{
		bool bRet = true;
		AVSampleFormat target_format = AV_SAMPLE_FMT_NONE;
		switch (audio_format)
		{
		case AUDIO_FORMAT_U8BIT:
			av_sample_fmt = AV_SAMPLE_FMT_U8;
			break;
		case AUDIO_FORMAT_16BIT:
			av_sample_fmt = AV_SAMPLE_FMT_S16;
			break;
		case AUDIO_FORMAT_32BIT:
			av_sample_fmt = AV_SAMPLE_FMT_S32;
			break;
		case AUDIO_FORMAT_FLOAT:
			av_sample_fmt = AV_SAMPLE_FMT_FLT;
			break;
		case AUDIO_FORMAT_U8BIT_PLANAR:
			av_sample_fmt = AV_SAMPLE_FMT_U8P;
			break;
		case AUDIO_FORMAT_16BIT_PLANAR:
			av_sample_fmt = AV_SAMPLE_FMT_S16P;
			break;
		case AUDIO_FORMAT_32BIT_PLANAR:
			av_sample_fmt = AV_SAMPLE_FMT_S32P;
			break;
		case AUDIO_FORMAT_FLOAT_PLANAR:
			av_sample_fmt = AV_SAMPLE_FMT_FLTP;
			break;
		default:
			bRet = false;
		}
		return bRet;
	}

	bool ConvertToAVChannelLayOut(MediaFileRecorder::CHANNEL_LAYOUT channel_lay_out, int64_t& av_chl_layout)
	{
		bool bRet = true;
		switch (channel_lay_out)
		{
		case SPEAKERS_MONO:
			av_chl_layout = AV_CH_LAYOUT_MONO;
			break;
		case SPEAKERS_STEREO:
			av_chl_layout = AV_CH_LAYOUT_STEREO;
			break;
		case SPEAKERS_2POINT1:
			av_chl_layout = AV_CH_LAYOUT_2_1;
			break;
		case SPEAKERS_QUAD:
			av_chl_layout = AV_CH_LAYOUT_QUAD;
			break;
		case SPEAKERS_4POINT1:
			av_chl_layout = AV_CH_LAYOUT_4POINT1;
			break;
		case SPEAKERS_5POINT1:
			av_chl_layout = AV_CH_LAYOUT_5POINT1;
			break;
		case SPEAKERS_5POINT1_SURROUND:
			av_chl_layout = AV_CH_LAYOUT_5POINT1_BACK;
			break;
		case SPEAKERS_7POINT1:
			av_chl_layout = AV_CH_LAYOUT_7POINT1;
			break;
		case SPEAKERS_7POINT1_SURROUND:
			av_chl_layout = AV_CH_LAYOUT_7POINT1_WIDE_BACK;
			break;
		case SPEAKERS_SURROUND:
			av_chl_layout = AV_CH_LAYOUT_SURROUND;
			break;
		default:
			bRet = false;
		}
		return bRet;
	}

	bool ConvertToAVPixelFormat(PIX_FMT pix_fmt, AVPixelFormat& av_pix_fmt)
	{
		bool bRet = true;
		switch (pix_fmt)
		{
		case PIX_FMT::PIX_FMT_ARGB:
			av_pix_fmt = AV_PIX_FMT_ARGB;
			break;
		case PIX_FMT::PIX_FMT_BGRA:
			av_pix_fmt = AV_PIX_FMT_BGRA;
			break;
		case PIX_FMT::PIX_FMT_BGR24:
			av_pix_fmt = AV_PIX_FMT_BGR24;
			break;
		case PIX_FMT::PIX_FMT_RGB24:
			av_pix_fmt = AV_PIX_FMT_RGB24;
			break;
		default:
			bRet = false;
			break;
		}

		return bRet;
	}

}

