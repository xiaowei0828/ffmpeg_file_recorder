#include "stdafx.h"
#include "MediaFileRecorder.h"
#include <windows.h>

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
	m_nAudioFrameIndex(0)
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
	m_pVideoCodecCtx->bit_rate = m_stRecordInfo.video_info.bit_rate;
	m_pVideoCodecCtx->time_base.num = 1;
	m_pVideoCodecCtx->time_base.den = m_stRecordInfo.video_info.frame_rate;
	//default setting?
	m_pVideoCodecCtx->gop_size = 250;
	m_pVideoCodecCtx->qmin = 10;
	m_pVideoCodecCtx->qmax = 51;
	//optional param
	m_pVideoCodecCtx->max_b_frames = 3;
	// Set option
	AVDictionary *param = 0;
	av_dict_set(&param, "preset", "slow", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);

	AVCodec* pEncoder = avcodec_find_encoder(m_pVideoCodecCtx->codec_id);
	if (!pEncoder)
	{
		OutputDebugStringA("avcodec_findo_encoder failed\n");
		return false;
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

	//ÉêÇë30Ö¡»º´æ
	m_pVideoFifoBuffer = av_fifo_alloc(30 * avpicture_get_size(m_pVideoCodecCtx->pix_fmt,
		m_pVideoCodecCtx->width, m_pVideoCodecCtx->height));

	InitializeCriticalSection(&m_VideoSection);

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
}

bool CMediaFileRecorder::InitAudioRecord()
{
	return true;
}

void CMediaFileRecorder::UnInitAudioRecord()
{
	return;
}


void CMediaFileRecorder::UnInit()
{
	StopWriteFileThread();
	av_write_trailer(m_pFormatCtx);
	UnInitVideoRecord();
	UnInitAudioRecord();
	avio_close(m_pFormatCtx->pb);
	avformat_free_context(m_pFormatCtx);
	m_pFormatCtx = NULL;
	m_bInited = false;
}

bool CMediaFileRecorder::Start()
{
	if (!m_bInited)
	{
		OutputDebugStringA("Not inited\n");
		return false;
	}
	StartWriteFileThread();
}

void CMediaFileRecorder::Stop()
{
	if (m_bRun)
	{
		StopWriteFileThread();
	}
}

void CMediaFileRecorder::FillVideo(void* data)
{
	if (m_bInited && m_bRun)
	{
		if (av_fifo_space(m_pVideoFifoBuffer) >= m_nPicSize)
		{
			SwsContext* pConvertCtx = sws_getContext(m_stRecordInfo.video_info.src_width,
				m_stRecordInfo.video_info.src_height,
				m_stRecordInfo.video_info.src_pix_fmt,
				m_stRecordInfo.video_info.dst_width,
				m_stRecordInfo.video_info.dst_height,
				AV_PIX_FMT_YUV420P,
				SWS_BICUBIC, NULL, NULL, NULL);
			uint8_t* src_rgb[3] = {(uint8_t*)data, NULL, NULL};
			int src_rgb_stride[3] = {3*m_stRecordInfo.video_info.src_width, 0 ,0};
			sws_scale(pConvertCtx, src_rgb, src_rgb_stride, 0, 3 * m_stRecordInfo.video_info.src_height,
				m_pInVideoFrame->data, m_pInVideoFrame->linesize);

			int size = m_stRecordInfo.video_info.dst_width * m_stRecordInfo.video_info.dst_height;
			EnterCriticalSection(&m_VideoSection);
			av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[0], size, NULL);
			av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[1], size / 4, NULL);
			av_fifo_generic_write(m_pVideoFifoBuffer, m_pInVideoFrame->data[2], size / 4, NULL);
			LeaveCriticalSection(&m_VideoSection);
		}
	}
	
}

void CMediaFileRecorder::FillAudio()
{

}

void CMediaFileRecorder::StartWriteFileThread()
{
	m_bRun = true;
	m_WriteFileThread.swap(std::thread(std::bind(&CMediaFileRecorder::WriteFileThreadProc, this)));
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
		if (av_fifo_size(m_pVideoFifoBuffer) >= m_nPicSize)
		{
			EnterCriticalSection(&m_VideoSection);
			av_fifo_generic_read(m_pVideoFifoBuffer, m_pOutPicBuffer, m_nPicSize, NULL);
			LeaveCriticalSection(&m_VideoSection);
			/*avpicture_fill((AVPicture*)m_pVideoFrame, m_pPicBuffer,
				m_pVideoCodecCtx->pix_fmt,
				m_pVideoCodecCtx->width,
				m_pVideoCodecCtx->height);*/
				
			m_pOutVideoFrame->pts = m_nVideoFrameIndex * 
				((m_pFormatCtx->streams[0]->time_base.den / m_pFormatCtx->streams[0]->time_base.num) / m_stRecordInfo.video_info.frame_rate);
			
			int bGotPicture = 0;

			int ret = avcodec_encode_video2(m_pVideoCodecCtx, &m_VideoPacket, m_pOutVideoFrame, &bGotPicture);
			if (ret < 0)
			{
				continue;
			}

			if (bGotPicture == 1)
			{
				m_VideoPacket.stream_index = 0;
				m_nCurrVideoPts = m_VideoPacket.pts;

				ret = av_write_frame(m_pFormatCtx, &m_VideoPacket);
			}
			m_nVideoFrameIndex++;
		}
	}
}







