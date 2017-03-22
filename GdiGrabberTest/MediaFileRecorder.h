#ifndef MEDIAFILERECORDER_H
#define MEDIAFILERECORDER_H

#ifdef	__cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")

	//#pragma comment(lib, "avfilter.lib")
	//#pragma comment(lib, "postproc.lib")
	//#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
#ifdef __cplusplus
};
#endif

#include <thread>
#include <atomic>


class CMediaFileRecorder
{
public:
	struct RecordInfo
	{
		char file_name[1024];
		struct Video
		{
			int src_width;
			int src_height;
			int dst_width;
			int dst_height;
			int frame_rate;
			int bit_rate;
			AVPixelFormat src_pix_fmt;
		}video_info;

		struct Audio
		{

		}audio_info;
		
		RecordInfo()
		{
			memset(file_name, 0, 1024);
			video_info.src_width = 0;
			video_info.src_height = 0;
			video_info.dst_width = 0;
			video_info.dst_height = 0;
			video_info.frame_rate = 0;
			video_info.bit_rate = 0;
			video_info.src_pix_fmt = AV_PIX_FMT_NONE;
		}
	};
public:
	CMediaFileRecorder();
	~CMediaFileRecorder();

	bool Init(const RecordInfo& record_info);
	void UnInit();
	bool Start();
	void Stop();
	void FillVideo(void* data);
	void FillAudio();

private:
	bool InitVideoRecord();
	void UnInitVideoRecord();
	bool InitAudioRecord();
	void UnInitAudioRecord();
	void StartWriteFileThread();
	void StopWriteFileThread();
	void WriteFileThreadProc();

private:
	bool m_bInited;
	std::atomic_bool m_bRun;
	RecordInfo m_stRecordInfo;
	AVFormatContext* m_pFormatCtx;
	AVStream* m_pVideoStream;
	AVStream* m_pAudioStream;
	AVCodecContext* m_pVideoCodecCtx;
	AVCodecContext* m_pAudioCodecCtx;
	AVPacket m_VideoPacket;
	AVPacket m_AudioPacket;
	int m_nPicSize;
	AVFrame* m_pOutVideoFrame;
	uint8_t* m_pOutPicBuffer;
	AVFrame* m_pInVideoFrame;
	uint8_t* m_pInPicBuffer;
	AVFrame* m_pAudioFrame;

	uint8_t* m_pAudioBuffer;
	int m_nAudioSize;

	AVFifoBuffer* m_pVideoFifoBuffer;
	AVFifoBuffer* m_pAudioFifoBuffer;

	CRITICAL_SECTION m_VideoSection;
	CRITICAL_SECTION m_AudiSection;

	int64_t m_nCurrVideoPts;
	int64_t m_nCurrAudioPts;
	int64_t m_nVideoFrameIndex;
	int64_t m_nAudioFrameIndex;

	std::thread m_WriteFileThread;
	
};


#endif