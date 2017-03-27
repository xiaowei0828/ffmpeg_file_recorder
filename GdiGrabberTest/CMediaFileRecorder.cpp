#include "stdafx.h"
#include "CMediaFileRecorder.h"
#include <windows.h>
#include <timeapi.h>


void av_log_cb(void* data, int level, const char* msg, va_list args)
{
	char log[1024] = { 0 };
	vsprintf(log, msg, args);
	OutputDebugStringA(log);
}

namespace MediaFileRecorder
{
	CMediaFileRecorder::CMediaFileRecorder()
		:m_bInited(false),
		m_pFormatCtx(NULL),
		m_pVideoStream(NULL),
		m_pAudioStream(NULL),
		m_pVideoCodecCtx(NULL),
		m_pAudioCodecCtx(NULL),
		m_pOutVideoFrame(NULL),
		m_pOutPicBuffer(NULL),
		m_pInVideoFrame(NULL),
		m_pInPicBuffer(NULL),
		m_pAudioFrame(NULL),
		m_nPicSize(0),
		m_pAudioBuffer(NULL),
		m_nAudioSize(0),
		m_pVideoFifoBuffer(NULL),
		m_pAudioFifoBuffer(NULL),
		m_nCurrAudioPts(0),
		m_nVideoFrameIndex(0),
		m_nAudioFrameIndex(0),
		m_pVideoConvertCtx(NULL),
		m_nStartTime(0),
		m_nDuration(0),
		m_pAudioConvertCtx(NULL),
		m_nSavedAudioSamples(0),
		m_nDiscardAudioSamples(0)
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

	int CMediaFileRecorder::Init(const RecordInfo& record_info)
	{
		if (m_bInited)
		{
			OutputDebugStringA("Inited already!\n");
			return -1;
		}

		m_stRecordInfo = record_info;

		int ret = 0;
		av_register_all();

		ret = avformat_alloc_output_context2(&m_pFormatCtx, NULL, NULL, m_stRecordInfo.file_name);
		if (ret < 0)
		{
			OutputDebugStringA("avformat_alloc_output_context2\n");
			return -1;
		}

		ret = avio_open(&m_pFormatCtx->pb, m_stRecordInfo.file_name, AVIO_FLAG_READ_WRITE);
		if (ret < 0)
		{
			OutputDebugStringA("avio_open failed\n");
			return -1;
		}

		if (!InitVideoRecord())
		{
			OutputDebugStringA("Init video record failed\n");
			return -1;
		}

		if (!InitAudioRecord())
		{
			OutputDebugStringA("Init audio record failed\n");
			return -1;
		}

		InitializeCriticalSection(&m_WriteFileSection);

		//write file header
		avformat_write_header(m_pFormatCtx, NULL);

		m_bInited = true;
		
		return 0;
	}


	bool CMediaFileRecorder::InitVideoRecord()
	{
		int ret = 0;

		m_pVideoStream = avformat_new_stream(m_pFormatCtx, NULL);
		if (m_pVideoStream == NULL)
		{
			OutputDebugStringA("Create new video stream failed\n");
			return false;
		}

		m_pVideoCodecCtx = m_pVideoStream->codec;
		m_pVideoCodecCtx->codec_id = AV_CODEC_ID_H264;
		m_pVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
		m_pVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
		m_pVideoCodecCtx->width = m_stRecordInfo.video_info.dst_width;
		m_pVideoCodecCtx->height = m_stRecordInfo.video_info.dst_height;
		//m_pVideoCodecCtx->bit_rate = m_stRecordInfo.video_info.bit_rate;
		m_pVideoCodecCtx->flags &= CODEC_FLAG_QSCALE;
		/*m_pVideoCodecCtx->rc_min_rate = 128000;
		m_pVideoCodecCtx->rc_max_rate = 8 * 50000;*/

		m_pVideoCodecCtx->time_base.num = 1;
		m_pVideoCodecCtx->time_base.den = m_stRecordInfo.video_info.frame_rate;
		m_pVideoCodecCtx->qmin = 18;
		m_pVideoCodecCtx->qmax = 28;

		m_pVideoCodecCtx->gop_size = m_stRecordInfo.video_info.frame_rate * 10;
		m_pVideoCodecCtx->max_b_frames = 0;
		m_pVideoCodecCtx->delay = 0;
		m_pVideoCodecCtx->thread_count = 10;
		//m_pVideoCodecCtx->gop_size = 10;
		//default setting?
		//m_pVideoCodecCtx->gop_size = 250;

		//optional param

		//m_pVideoCodecCtx->flags &= CODEC_FLAG_QSCALE;
		// Set option
		AVDictionary *param = 0;
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);

		AVCodec* pEncoder = avcodec_find_encoder(m_pVideoCodecCtx->codec_id);
		if (!pEncoder)
		{
			OutputDebugStringA("avcodec_findo_encoder failed\n");
			return false;
		}

		if (m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			m_pVideoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		ret = avcodec_open2(m_pVideoCodecCtx, pEncoder, &param);
		if (ret < 0)
		{
			OutputDebugStringA("avcodec_open2 failed\n");
			return false;
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

		av_new_packet(&m_VideoPacket, m_nPicSize);

		//申请30帧缓存
		m_pVideoFifoBuffer = av_fifo_alloc(30 * m_nPicSize);

		InitializeCriticalSection(&m_VideoSection);

		m_nWriteFrame = 0;
		m_nDiscardFrame = 0;

		return true;
	}

	void CMediaFileRecorder::UnInitVideoRecord()
	{
		av_free_packet(&m_VideoPacket);
		avcodec_close(m_pVideoCodecCtx);
		m_pVideoCodecCtx = NULL;
		av_free(m_pOutVideoFrame);
		m_pOutVideoFrame = NULL;
		av_free(m_pOutPicBuffer);
		m_pOutPicBuffer = NULL;
		av_free(m_pInVideoFrame);
		m_pInVideoFrame = NULL;
		av_free(m_pInPicBuffer);
		m_pInPicBuffer = NULL;
		av_fifo_free(m_pVideoFifoBuffer);
		m_pVideoFifoBuffer = NULL;
		sws_freeContext(m_pVideoConvertCtx);
		m_pVideoConvertCtx = NULL;

		m_nPicSize = 0;
		m_nVideoFrameIndex = 0;

		char log[128] = { 0 };
		_snprintf(log, 128, "Write video count: %lld, Discard frame count: %lld \n",
			m_nWriteFrame, m_nDiscardFrame);
		OutputDebugStringA(log);
	}

	bool CMediaFileRecorder::InitAudioRecord()
	{
		m_pAudioStream = avformat_new_stream(m_pFormatCtx, NULL);
		if (!m_pAudioStream)
		{
			OutputDebugStringA("create audio stream failed! \n");
			return false;
		}

		m_pAudioCodecCtx = m_pAudioStream->codec;
		m_pAudioCodecCtx->codec_id = AV_CODEC_ID_AAC;
		m_pAudioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
		m_pAudioCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
		m_pAudioCodecCtx->sample_rate = 44100;
		m_pAudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
		m_pAudioCodecCtx->channels = av_get_channel_layout_nb_channels(m_pAudioCodecCtx->channel_layout);
		//m_pAudioCodecCtx->bit_rate = 128000;

		AVCodec* audio_encoder = avcodec_find_encoder(m_pAudioCodecCtx->codec_id);
		if (!audio_encoder)
		{
			OutputDebugStringA("failed to find audio encoder! \n");
			return false;
		}

		if (m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			m_pAudioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		if (avcodec_open2(m_pAudioCodecCtx, audio_encoder, NULL) < 0)
		{
			OutputDebugStringA("failed to open audio codec context");
			return false;
		}

		m_pAudioFrame = av_frame_alloc();
		m_pAudioFrame->nb_samples = m_pAudioCodecCtx->frame_size;
		m_pAudioFrame->format = m_pAudioCodecCtx->sample_fmt;

		m_nAudioSize = av_samples_get_buffer_size(NULL, m_pAudioCodecCtx->channels,
			m_pAudioCodecCtx->frame_size, m_pAudioCodecCtx->sample_fmt, 1);
		m_pAudioBuffer = (uint8_t *)av_malloc(m_nAudioSize);
		avcodec_fill_audio_frame(m_pAudioFrame, m_pAudioCodecCtx->channels,
			m_pAudioCodecCtx->sample_fmt, (const uint8_t*)m_pAudioBuffer, m_nAudioSize, 1);

		av_new_packet(&m_AudioPacket, m_nAudioSize);

		//申请100帧缓存
		m_pAudioFifoBuffer = av_audio_fifo_alloc(m_pAudioCodecCtx->sample_fmt,
			m_pAudioCodecCtx->channels, 100 * m_pAudioFrame->nb_samples);

		InitializeCriticalSection(&m_AudioSection);

		m_pAudioConvertCtx = swr_alloc();

		return true;
	}

	void CMediaFileRecorder::UnInitAudioRecord()
	{
		av_free_packet(&m_AudioPacket);
		avcodec_close(m_pAudioCodecCtx);
		m_pAudioCodecCtx = NULL;
		av_free(m_pAudioFrame);
		m_pAudioFrame = NULL;
		av_free(m_pAudioBuffer);
		m_pAudioBuffer = NULL;
		av_audio_fifo_free(m_pAudioFifoBuffer);
		m_pAudioFifoBuffer = NULL;
		swr_free(&m_pAudioConvertCtx);
		m_pAudioConvertCtx = NULL;

		char log[128] = { 0 };
		_snprintf(log, 128, "Write audio frame count: %lld, save samples: %lld, discard samples: %lld\n",
			m_nAudioFrameIndex, m_nSavedAudioSamples, m_nDiscardAudioSamples);
		OutputDebugStringA(log);
		m_nAudioSize = 0;
		m_nAudioFrameIndex = 0;
		m_nSavedAudioSamples = 0;
		m_nDiscardAudioSamples = 0;


		return;
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
			avio_close(m_pFormatCtx->pb);
			avformat_free_context(m_pFormatCtx);
			m_pFormatCtx = NULL;
			m_bInited = false;

			m_nStartTime = 0;
			m_nDuration = 0;

			return 0;
		}
		return -1;
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

	int CMediaFileRecorder::FillVideo(const void* data, int width, int height, PIX_FMT pix_fmt)
	{
		int ret = -1;
		if (m_bInited && m_bRun)
		{
			int64_t begin_time = timeGetTime();

			int dst_width = m_stRecordInfo.video_info.dst_width;
			int dst_height = m_stRecordInfo.video_info.dst_height;
			AVPixelFormat dst_pix_fmt = AV_PIX_FMT_YUV420P;

			AVPixelFormat av_pix_fmt;
			if (!ConvertToAVPixelFormat(pix_fmt, av_pix_fmt))
			{
				OutputDebugStringA("FillVideo: convert failed \n");
				return -1;
			}

			if (av_fifo_space(m_pVideoFifoBuffer) >= m_nPicSize + sizeof(int64_t))
			{
				if (!m_pVideoConvertCtx)
				{
					
					m_pVideoConvertCtx = sws_getContext(width,
						height, av_pix_fmt,
						dst_width, dst_height,
						dst_pix_fmt,
						NULL, NULL,
						NULL, NULL);
				}
				int bytes_per_pix;
				if (pix_fmt == PIX_FMT_ARGB || pix_fmt  == PIX_FMT_BGRA)
					bytes_per_pix = 4;
				else if (pix_fmt == PIX_FMT_BGR24 | pix_fmt == PIX_FMT_RGB24)
					bytes_per_pix = 3;

				uint8_t* srcSlice[3] = { (uint8_t*)data, NULL, NULL };
				int srcStride[3] = { bytes_per_pix * width, 0, 0 };
				sws_scale(m_pVideoConvertCtx, srcSlice, srcStride, 0, height,
					m_pInVideoFrame->data, m_pInVideoFrame->linesize);

				int64_t nVideoCaptureTime;
				nVideoCaptureTime = m_nDuration + (timeGetTime() - m_nStartTime);
				int size = dst_width * dst_height;
				EnterCriticalSection(&m_VideoSection);
				av_fifo_generic_write(m_pVideoFifoBuffer, &nVideoCaptureTime, sizeof(int64_t), NULL);
				av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[0], size, NULL);
				av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[1], size / 4, NULL);
				av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[2], size / 4, NULL);
				LeaveCriticalSection(&m_VideoSection);
				m_nWriteFrame++;

				ret = 0;
			}
			else
			{
				m_nDiscardFrame++;
			}
			int64_t duration = timeGetTime() - begin_time;
			char log[128] = { 0 };
			_snprintf(log, 128, "FillVideo spend time: %lld \n", duration);
			OutputDebugStringA(log);
		}
		return ret;
	}


	int CMediaFileRecorder::FillAudio(const void* audioSamples,
		int nb_samples, int sample_rate,
		CHANNEL_LAYOUT chl_layout, AUDIO_FORMAT sample_fmt)
	{
		int ret = -1;
		if (m_bInited & m_bRun)
		{
			int64_t begin_time = timeGetTime();

			int src_rate = sample_rate;
			int src_nb_samples = nb_samples;
			AVSampleFormat src_av_sample_fmt;
			if (!ConvertToAVSampleFormat(sample_fmt, src_av_sample_fmt))
			 {
				 OutputDebugStringA("FiilAudio: convert to av sample format failed \n");
				 return -1;
			 }
			 ;
			int src_nb_channels = av_get_channel_layout_nb_channels(src_av_sample_fmt);

			int64_t src_av_ch_layout;
			if (!ConvertToAVChannelLayOut(chl_layout, src_av_ch_layout))
			{
				OutputDebugStringA("FillAudio: convert to av channel layout failed \n");
				return -1;
			}

			if (src_rate != m_pAudioCodecCtx->sample_rate ||
				src_av_ch_layout != m_pAudioCodecCtx->channel_layout ||
				src_av_sample_fmt != m_pAudioCodecCtx->sample_fmt)
			{
				ret = ResampleAndSave(audioSamples, src_nb_samples,
					src_rate, src_nb_channels, src_av_ch_layout, src_av_sample_fmt);
			}
			else
			{
				if (av_audio_fifo_space(m_pAudioFifoBuffer) >= src_nb_samples)
				{
					EnterCriticalSection(&m_AudioSection);
					av_audio_fifo_write(m_pAudioFifoBuffer, (void **)&audioSamples, src_nb_samples);
					LeaveCriticalSection(&m_AudioSection);
					m_nSavedAudioSamples += src_nb_samples;

					ret = 0;
				}
				else
				{
					OutputDebugStringA("Discard audio samples");
					m_nDiscardAudioSamples += src_nb_samples;
				}
			}

			/*int64_t spend_time = timeGetTime() - begin_time;
			char log[128] = { 0 };
			sprintf(log, "FillAudio spend time: %lld \n", spend_time);
			OutputDebugStringA(log);*/
		}

		return ret;
	}

	int CMediaFileRecorder::ResampleAndSave(const void* audioSamples,
		int src_nb_samples, int src_rate,
		int src_nb_channels, int src_ch_layout,
		AVSampleFormat src_sample_fmt)
	{
		int ret = -1;

		int64_t begin_time = timeGetTime();

		/* set options */
		//设置源通道布局  
		av_opt_set_int(m_pAudioConvertCtx, "in_channel_layout", src_ch_layout, 0);
		//设置源通道采样率  
		av_opt_set_int(m_pAudioConvertCtx, "in_sample_rate", src_rate, 0);
		//设置源通道样本格式  
		av_opt_set_sample_fmt(m_pAudioConvertCtx, "in_sample_fmt", src_sample_fmt, 0);

		//目标通道布局  
		av_opt_set_int(m_pAudioConvertCtx, "out_channel_layout", m_pAudioCodecCtx->channel_layout, 0);
		//目标采用率  
		av_opt_set_int(m_pAudioConvertCtx, "out_sample_rate", m_pAudioCodecCtx->sample_rate, 0);
		//目标样本格式  
		av_opt_set_sample_fmt(m_pAudioConvertCtx, "out_sample_fmt", m_pAudioCodecCtx->sample_fmt, 0);

		if (swr_init(m_pAudioConvertCtx) < 0)
		{
			OutputDebugStringA("swr_init failed in FillAudio. \n");
			return -1;
		}

		int max_dst_nb_samples = 0;
		int dst_nb_samples = 0;
		max_dst_nb_samples = dst_nb_samples = av_rescale_rnd(src_nb_samples,
			m_pAudioCodecCtx->sample_rate, src_rate, AV_ROUND_UP);

		uint8_t** dst_data = NULL;
		int dst_linsize;
		if (av_samples_alloc_array_and_samples(&dst_data, &dst_linsize,
			m_pAudioCodecCtx->channels, dst_nb_samples, m_pAudioCodecCtx->sample_fmt, 0) < 0)
		{
			OutputDebugStringA("av_samples_alloc_array_and_samples failed. \n");
			return -1;
		}

		dst_nb_samples = av_rescale_rnd(swr_get_delay(m_pAudioConvertCtx, src_rate) +
			src_nb_samples, m_pAudioCodecCtx->sample_rate, src_rate, AV_ROUND_UP);
		if (dst_nb_samples <= 0)
		{
			OutputDebugStringA("av_rescale_rnd failed. \n");
			return -1;
		}

		if (dst_nb_samples > max_dst_nb_samples)
		{
			av_free(dst_data[0]);
			av_samples_alloc(dst_data, &dst_linsize, m_pAudioCodecCtx->channels,
				dst_nb_samples, m_pAudioCodecCtx->sample_fmt, 1);
			max_dst_nb_samples = dst_nb_samples;
		}

		int ret_samples = swr_convert(m_pAudioConvertCtx, dst_data, dst_nb_samples,
			(const uint8_t**)&audioSamples, src_nb_samples);
		if (ret_samples <= 0)
		{
			OutputDebugStringA("swr_convert failed. \n");
			return -1;
		}

		int resampled_data_size = av_samples_get_buffer_size(&dst_linsize,
			m_pAudioCodecCtx->channels, ret_samples, m_pAudioCodecCtx->sample_fmt, 1);
		if (resampled_data_size <= 0)
		{
			OutputDebugStringA("av_samples_get_buffer_size failed. \n");
			return -1;
		}

		if (av_audio_fifo_space(m_pAudioFifoBuffer) >= ret_samples)
		{
			EnterCriticalSection(&m_AudioSection);
			av_audio_fifo_write(m_pAudioFifoBuffer, (void **)dst_data, ret_samples);
			LeaveCriticalSection(&m_AudioSection);
			m_nSavedAudioSamples += src_nb_samples;
			ret = 0;
		}
		else
		{
			m_nDiscardAudioSamples += src_nb_samples;
		}

		int64_t resample_time = timeGetTime() - begin_time;

		char log[128] = { 0 };
		sprintf(log, "resample time: %lld \n", resample_time);
		OutputDebugStringA(log);

		if (dst_data)
		{
			av_freep(&dst_data[0]);
		}
		av_freep(&dst_data);
		dst_data = NULL;
		return ret;
	}

	void CMediaFileRecorder::StartWriteFileThread()
	{
		m_bRun = true;
		m_WriteVideoThread.swap(std::thread(std::bind(&CMediaFileRecorder::VideoWriteFileThreadProc, this)));
		SetThreadPriority(m_WriteVideoThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

		m_WriteAudioThread.swap(std::thread(std::bind(&CMediaFileRecorder::AuidoWriteFileThreadProc, this)));
		SetThreadPriority(m_WriteAudioThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
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
				sprintf(log, "total size: %d kb, space: %d kb, used: %d kb, grow: %d kb \n",
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
				sprintf(log, "total size: %d kb, space: %d kb, used: %d kb, shrink: %d kb \n",
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
		while (av_fifo_size(m_pVideoFifoBuffer) >= m_nPicSize + sizeof(int64_t))
		{
			OutputDebugStringA("Write the left video data to file \n");
			EncodeAndWriteVideo();
		}
	}

	void CMediaFileRecorder::EncodeAndWriteVideo()
	{
		int64_t begin_time = timeGetTime();

		if (av_fifo_size(m_pVideoFifoBuffer) >= m_nPicSize + sizeof(int64_t))
		{
			int64_t nCaptureTime;
			EnterCriticalSection(&m_VideoSection);
			av_fifo_generic_read(m_pVideoFifoBuffer, &nCaptureTime, sizeof(int64_t), NULL);
			av_fifo_generic_read(m_pVideoFifoBuffer, m_pOutPicBuffer, m_nPicSize, NULL);
			LeaveCriticalSection(&m_VideoSection);

			//m_pOutVideoFrame->pts = nCaptureTime *
			//	((m_pVideoStream->time_base.den / m_pVideoStream->time_base.num)) / 1000;

			int bGotPicture = 0;

			int64_t encode_start_time = timeGetTime();
			int ret = avcodec_encode_video2(m_pVideoCodecCtx, &m_VideoPacket, m_pOutVideoFrame, &bGotPicture);
			int64_t encode_duration = timeGetTime() - encode_start_time;

			if (ret < 0)
			{
				return;
			}

			if (bGotPicture == 1)
			{
				m_VideoPacket.stream_index = 0;
				m_VideoPacket.pts = nCaptureTime * ((m_pVideoStream->time_base.den / m_pVideoStream->time_base.num)) / 1000;
				m_VideoPacket.dts = m_VideoPacket.pts;
				//m_VideoPacket.duration = 1;

				EnterCriticalSection(&m_WriteFileSection);
				ret = av_interleaved_write_frame(m_pFormatCtx, &m_VideoPacket);
				LeaveCriticalSection(&m_WriteFileSection);

				char log[128] = { 0 };
				_snprintf(log, 128, "encode frame: %lld\n", encode_duration);
				OutputDebugStringA(log);
			}
			m_nVideoFrameIndex++;
		}
		int64_t duration = timeGetTime() - begin_time;

		char log[128] = { 0 };
		sprintf(log, "Write video file time: %lld, used size: %d, space: %d \n",
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

		// write the left audio data to file
		while (av_audio_fifo_size(m_pAudioFifoBuffer) >= m_pAudioCodecCtx->frame_size)
		{
			EncodeAndWriteAudio();
		}
	}

	void CMediaFileRecorder::EncodeAndWriteAudio()
	{
		int64_t begin_time = timeGetTime();
		if (av_audio_fifo_size(m_pAudioFifoBuffer) >= m_pAudioCodecCtx->frame_size)
		{
			EnterCriticalSection(&m_AudioSection);
			av_audio_fifo_read(m_pAudioFifoBuffer, (void**)m_pAudioFrame->data,
				m_pAudioCodecCtx->frame_size);
			LeaveCriticalSection(&m_AudioSection);

			int got_picture = 0;
			m_pAudioFrame->pts = m_nAudioFrameIndex * m_pAudioCodecCtx->frame_size;
			if (avcodec_encode_audio2(m_pAudioCodecCtx, &m_AudioPacket, m_pAudioFrame, &got_picture) < 0)
			{
				OutputDebugStringA("avcodec_encode_audio2 failed");
			}
			if (got_picture)
			{
				int64_t pts = (m_nDuration + (timeGetTime() - m_nStartTime)) * m_pAudioCodecCtx->sample_rate / 1000;
				m_AudioPacket.stream_index = 1;
				m_AudioPacket.pts = pts/*m_nAudioFrameIndex * m_pAudioCodecCtx->frame_size*/;
				m_AudioPacket.dts = pts/*m_nAudioFrameIndex * m_pAudioCodecCtx->frame_size*/;
				m_AudioPacket.duration = m_pAudioCodecCtx->frame_size;


				//cur_pts_a = m_AudioPacket.pts;

				EnterCriticalSection(&m_WriteFileSection);
				av_interleaved_write_frame(m_pFormatCtx, &m_AudioPacket);
				LeaveCriticalSection(&m_WriteFileSection);
				m_nAudioFrameIndex++;

				/*char log[128] = { 0 };
				sprintf(log, "audio frame count: %lld \n", m_nAudioFrameIndex);
				OutputDebugStringA(log);*/
			}
		}
		/*int64_t duration = timeGetTime() - begin_time;
		char log[128] = { 0 };
		sprintf(log, "Write audio file: %lld \n", duration);
		OutputDebugStringA(log);*/
	}


	bool CMediaFileRecorder::ConvertToAVSampleFormat(AUDIO_FORMAT audio_format, AVSampleFormat& av_sample_fmt)
	{
		bool bRet = true;
		AVSampleFormat target_format = AV_SAMPLE_FMT_NONE;
		switch (audio_format)
		{
		case AUDIO_FORMAT_UNKNOWN:
			av_sample_fmt = AV_SAMPLE_FMT_S16;
			break;
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

	bool CMediaFileRecorder::ConvertToAVChannelLayOut(MediaFileRecorder::CHANNEL_LAYOUT channel_lay_out, int64_t& av_chl_layout)
	{
		bool bRet = true;
		switch (channel_lay_out)
		{
		case SPEAKERS_UNKNOWN:          
			av_chl_layout = 0;
			break;
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

	bool CMediaFileRecorder::ConvertToAVPixelFormat(PIX_FMT pix_fmt, AVPixelFormat& av_pix_fmt)
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

