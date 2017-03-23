#include "stdafx.h"
#include "MediaFileRecorder.h"
#include <windows.h>
#include <timeapi.h>

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
	m_nCurrVideoPts(0),
	m_nCurrAudioPts(0),
	m_nVideoFrameIndex(0),
	m_nAudioFrameIndex(0),
	m_pConvertCtx(NULL),
	m_nStartTime(0),
	m_nDuration(0)
{
	m_bRun = false;
}

CMediaFileRecorder::~CMediaFileRecorder()
{
	if (m_bInited)
	{
		UnInit();
	}
}

bool CMediaFileRecorder::Init(const RecordInfo& record_info)
{
	if (m_bInited)
	{
		OutputDebugStringA("Inited already!\n");
		return false;
	}

	m_stRecordInfo = record_info;

	int ret = 0;
	av_register_all();

	ret = avformat_alloc_output_context2(&m_pFormatCtx, NULL, NULL, m_stRecordInfo.file_name);
	if (ret < 0)
	{
		OutputDebugStringA("avformat_alloc_output_context2\n");
		return false;
	}

	ret = avio_open(&m_pFormatCtx->pb, m_stRecordInfo.file_name, AVIO_FLAG_READ_WRITE);
	if (ret < 0)
	{
		OutputDebugStringA("avio_open failed\n");
		return false;
	}

	if (!InitVideoRecord())
	{
		OutputDebugStringA("Init video record failed\n");
		return false;
	}

	if (!InitAudioRecord())
	{
		OutputDebugStringA("Init audio record failed\n");
		return false;
	}

	//write file header
	avformat_write_header(m_pFormatCtx, NULL);

	m_bInited = true;
	return true;
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
	m_pVideoCodecCtx->qmin = 5;
	m_pVideoCodecCtx->qmax = 20;
	//default setting?
	/*m_pVideoCodecCtx->gop_size = 250;
	
	//optional param
	m_pVideoCodecCtx->max_b_frames = 3;*/

	//m_pVideoCodecCtx->flags &= CODEC_FLAG_QSCALE;
	// Set option
	AVDictionary *param = 0;
	av_dict_set(&param, "preset", "fast", 0);
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
	m_pVideoFifoBuffer = av_fifo_alloc(100 * m_nPicSize);

	InitializeCriticalSection(&m_VideoSection);

	m_pConvertCtx = sws_getContext(m_stRecordInfo.video_info.src_width,
		m_stRecordInfo.video_info.src_height,
		m_stRecordInfo.video_info.src_pix_fmt,
		m_stRecordInfo.video_info.dst_width,
		m_stRecordInfo.video_info.dst_height,
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, NULL, NULL, NULL);

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
	m_nPicSize = 0;
	m_nCurrVideoPts = 0;
	m_nVideoFrameIndex = 0;

	char log[128] = { 0 };
	_snprintf(log, 128, "Write frame count: %lld, Discar frame count: %lld \n",
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
	m_pAudioCodecCtx->bit_rate = 64000;

	AVCodec* audio_encoder = avcodec_find_encoder(m_pAudioCodecCtx->codec_id);
	if (!audio_encoder)
	{
		OutputDebugStringA("failed to find audio encoder! \n");
		return false;
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
	m_nAudioSize = 0;
	return;
}


void CMediaFileRecorder::UnInit()
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
	}
	
}

bool CMediaFileRecorder::Start()
{
	if (!m_bInited)
	{
		OutputDebugStringA("Not inited\n");
		return false;
	}
	m_nStartTime = timeGetTime();
	StartWriteFileThread();
	return true;
}

void CMediaFileRecorder::Stop()
{
	if (m_bRun)
	{
		StopWriteFileThread();
		m_nDuration += timeGetTime() - m_nStartTime;
	}
}

void CMediaFileRecorder::FillVideo(void* data)
{
	if (m_bInited && m_bRun)
	{
		int64_t begin_time = timeGetTime();
		if (av_fifo_space(m_pVideoFifoBuffer) >= m_nPicSize + sizeof(int64_t))
		{
			int64_t nVideoCaptureTime;

			nVideoCaptureTime = m_nDuration + (timeGetTime() - m_nStartTime);
			
			uint8_t* src_rgb[3] = { (uint8_t*)data, NULL, NULL};
			int src_rgb_stride[3] = { 3 * m_stRecordInfo.video_info.src_width,0, 0};
			sws_scale(m_pConvertCtx, src_rgb, src_rgb_stride, 0, 3 * m_stRecordInfo.video_info.src_height,
				m_pInVideoFrame->data, m_pInVideoFrame->linesize);

			int size = m_stRecordInfo.video_info.dst_width * m_stRecordInfo.video_info.dst_height;
			EnterCriticalSection(&m_VideoSection);
			av_fifo_generic_write(m_pVideoFifoBuffer, &nVideoCaptureTime, sizeof(int64_t), NULL);
			av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[0], size, NULL);
			av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[1], size / 4, NULL);
			av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[2], size / 4, NULL);
			LeaveCriticalSection(&m_VideoSection);
			m_nWriteFrame++;
		}
		else
		{
			m_nDiscardFrame++;
		}
		int64_t duration = timeGetTime() - begin_time;
		char log[128] = { 0 };
		_snprintf(log, 128, "fifo time: %lld \n", duration);
		OutputDebugStringA(log);
	}
	
}


void CMediaFileRecorder::FillAudio(const void* audioSamples, 
	const size_t nSamples, const size_t nBytesPerSample, 
	const uint8_t nChannels, const uint32_t samplesPerSec)
{
	int src_rate = samplesPerSec;
	int src_nb_channels = nChannels;
	int src_nb_samples = nSamples;
	int64_t src_ch_layout;
	if (nChannels == 1)
		src_ch_layout = AV_CH_LAYOUT_MONO;
	else if (nChannels == 2)
		src_ch_layout = AV_CH_LAYOUT_STEREO;
	else
	{
		OutputDebugStringA("Invalid nChannels in FillAudio. \n");
		return;
	}

	AVSampleFormat src_sample_fmt;
	if (nBytesPerSample == 2)
		src_sample_fmt = AV_SAMPLE_FMT_S16;
	else if (nBytesPerSample == 4)
		src_sample_fmt = AV_SAMPLE_FMT_S32;
	else
	{
		OutputDebugStringA("Invalid nBytesPerSample in FillAudio. \n");
		return;
	}

	SwrContext* swr_ctx = swr_alloc();
	if (!swr_ctx)
	{
		OutputDebugStringA("swr_alloc failed in FillAudio. \n");
		return;
	}

	/* set options */
	//设置源通道布局  
	av_opt_set_int(swr_ctx, "in_channel_layout", src_ch_layout, 0);
	//设置源通道采样率  
	av_opt_set_int(swr_ctx, "in_sample_rate", src_rate, 0);
	//设置源通道样本格式  
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

	//目标通道布局  
	av_opt_set_int(swr_ctx, "out_channel_layout", m_pAudioCodecCtx->channel_layout, 0);
	//目标采用率  
	av_opt_set_int(swr_ctx, "out_sample_rate", m_pAudioCodecCtx->sample_rate, 0);
	//目标样本格式  
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", m_pAudioCodecCtx->sample_fmt, 0);

	if (swr_init(swr_ctx) < 0)
	{
		OutputDebugStringA("swr_init failed in FillAudio. \n");
		return;
	}

	int max_dst_nb_samples = 0;
	int dst_nb_samples = 0;
	max_dst_nb_samples = dst_nb_samples = av_rescale_rnd(src_nb_samples,
		m_pAudioCodecCtx->sample_rate, src_rate, AV_ROUND_UP);

	uint8_t** dst_data = NULL;
	int src_linesize, dst_linsize;
	if (av_samples_alloc_array_and_samples(&dst_data, &dst_linsize, 
		m_pAudioCodecCtx->channels, dst_nb_samples, m_pAudioCodecCtx->sample_fmt, 0) < 0)
	{
		OutputDebugStringA("av_samples_alloc_array_and_samples failed. \n");
		return;
	}

	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) + 
		src_nb_samples, m_pAudioCodecCtx->sample_rate, src_rate, AV_ROUND_UP);
	if (dst_nb_samples <= 0)
	{
		OutputDebugStringA("av_rescale_rnd failed. \n");
		return;
	}

	if (dst_nb_samples > max_dst_nb_samples)
	{
		av_free(dst_data[0]);
		av_samples_alloc(dst_data, &dst_linsize, m_pAudioCodecCtx->channels,
			dst_nb_samples, m_pAudioCodecCtx->sample_fmt, 1);
		max_dst_nb_samples = dst_nb_samples;
	}

	int ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,
		(const uint8_t**)audioSamples, src_nb_samples);
	if (ret <= 0)
	{
		OutputDebugStringA("swr_convert failed. \n");
		return;
	}

	int resampled_data_size = av_samples_get_buffer_size(&dst_linsize,
		m_pAudioCodecCtx->channels, ret, m_pAudioCodecCtx->sample_fmt, 1);
	if (resampled_data_size <= 0)
	{
		OutputDebugStringA("av_samples_get_buffer_size failed. \n");
		return;
	}


}

void CMediaFileRecorder::StartWriteFileThread()
{
	m_bRun = true;
	m_WriteFileThread.swap(std::thread(std::bind(&CMediaFileRecorder::WriteFileThreadProc, this)));
	SetThreadPriority(m_WriteFileThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
}

void CMediaFileRecorder::StopWriteFileThread()
{
	m_bRun = false;
	if (m_WriteFileThread.joinable())
	{
		m_WriteFileThread.join();
	}
}

void CMediaFileRecorder::WriteFileThreadProc()
{
	while (m_bRun)
	{
		if (av_fifo_size(m_pVideoFifoBuffer) >= m_nPicSize + sizeof(int64_t))
		{
			int64_t nCaptureTime;
			EnterCriticalSection(&m_VideoSection);
			av_fifo_generic_read(m_pVideoFifoBuffer, &nCaptureTime, sizeof(int64_t), NULL);
			av_fifo_generic_read(m_pVideoFifoBuffer, m_pOutPicBuffer, m_nPicSize, NULL);
			LeaveCriticalSection(&m_VideoSection);
				
			m_pOutVideoFrame->pts = nCaptureTime *
				 ((m_pFormatCtx->streams[0]->time_base.den / m_pFormatCtx->streams[0]->time_base.num)) / 1000;
			
			int bGotPicture = 0;

			int64_t encode_start_time = timeGetTime();
			int ret = avcodec_encode_video2(m_pVideoCodecCtx, &m_VideoPacket, m_pOutVideoFrame, &bGotPicture);
			int64_t encode_duration = timeGetTime() - encode_start_time;

			if (ret < 0)
			{
				continue;
			}

			if (bGotPicture == 1)
			{
				m_VideoPacket.stream_index = 0;
				m_nCurrVideoPts = m_VideoPacket.pts;
				int64_t write_frame_time = timeGetTime();
				ret = av_interleaved_write_frame(m_pFormatCtx, &m_VideoPacket);
				int64_t write_frame_duration = timeGetTime() - write_frame_time;

				char log[128] = { 0 };
				_snprintf(log, 128, "encode frame: %lld, write frame: %lld \n", encode_duration, write_frame_duration);
				OutputDebugStringA(log);
			}
			m_nVideoFrameIndex++;
		}
	}
}







