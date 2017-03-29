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
#include "libswresample/swresample.h"

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "swresample.lib")
	//#pragma comment(lib, "avfilter.lib")
	//#pragma comment(lib, "postproc.lib")
	//#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
#ifdef __cplusplus
};
#endif

#include "IMediaFileRecorder.h"
#include <thread>
#include <atomic>
#include <vector>
#include <list>
#include <memory>

#define MAX_AV_PLANES 8

namespace MediaFileRecorder
{
	class CAudioRecordBuffer;

	class CMediaFileRecorder : public IMediaFileRecorder
	{
	public:
		CMediaFileRecorder();
		~CMediaFileRecorder();
		int Init(const RECORD_INFO& record_info) override;
		int UnInit() override;
		int Start() override;
		int Stop() override;
		int FillVideo(const void* data) override;
		int FillMicAudio(const void* audioSamples, int nb_samples) override;
		int FillSpeakerAudio(const void* audioSamples, int nb_samples) override;

	private:
		int InitVideoRecord();
		void UnInitVideoRecord();
		int InitAudioRecord();
		void UnInitAudioRecord();
		void StartWriteFileThread();
		void StopWriteFileThread();
		void VideoWriteFileThreadProc();
		void AuidoWriteFileThreadProc();

		void CleanUp();
		void VideoCleanUp();
		void AudioCleanUp();

		void EncodeAndWriteVideo();
		void EncodeAndWriteAudio();

		void MixAudio(AVFrame* pDstFrame, const AVFrame* pSrcFrame);

	private:
		bool m_bInited;
		std::atomic_bool m_bRun;
		RECORD_INFO m_stRecordInfo;
		AVFormatContext* m_pFormatCtx;
		AVCodecContext* m_pVideoCodecCtx;
		AVCodecContext* m_pAudioCodecCtx;
		AVPacket* m_pVideoPacket;
		AVPacket* m_pAudioPacket;
		int m_nPicSize;
		AVFrame* m_pOutVideoFrame;
		uint8_t* m_pOutPicBuffer;
		AVFrame* m_pInVideoFrame;
		uint8_t* m_pInPicBuffer;

		AVFrame* m_pAudioFrame;
		uint8_t* m_pAudioBuffer;
		uint32_t m_nAudioSize;

		SwsContext* m_pVideoConvertCtx;

		AVFifoBuffer* m_pVideoFifoBuffer;

		CRITICAL_SECTION m_VideoSection;

		int64_t m_nVideoFrameIndex;
		int64_t m_nAudioFrameIndex;

		std::thread m_WriteVideoThread;
		std::thread m_WriteAudioThread;

		int64_t m_nVideoWriteFrames;
		int64_t m_nVideoDiscardFrames;

		int64_t m_nStartTime;
		int64_t m_nDuration;

		CRITICAL_SECTION m_WriteFileSection;

		std::shared_ptr<CAudioRecordBuffer> m_pMicRecorder;
		std::shared_ptr<CAudioRecordBuffer> m_pSpeakerRecorder;
		uint32_t m_nVideoStreamIndex;
		uint32_t m_nAudioStreamIndex;
	};

	class CAudioRecordBuffer
	{
	public:
		CAudioRecordBuffer();
		~CAudioRecordBuffer();
		int Init(const AVCodecContext* pCodecCtx, const AUDIO_INFO& src_audio_info);
		int UnInit();
		void CleanUp();
		int FillAudio(const void* audioSamples, int nb_samples);
		AVFrame* GetOneFrame();
	private:
		std::atomic_bool m_bInited;
		const AVCodecContext* m_pCodecCtx;
		AUDIO_INFO m_stAudioInfo;
		SwrContext* m_pConvertCtx;
		AVAudioFifo* m_pFifoBuffer;
		AVFrame* m_pFrame;
		uint8_t* m_pFrameBuffer;
		uint8_t* m_pResampleBuffer[MAX_AV_PLANES];
		int m_nResampleBufferSize;
		CRITICAL_SECTION m_BufferSection;
		int64_t m_nSavedAudioSamples;
		int64_t m_nDiscardAudioSamples;
	};


	static inline bool ConvertToAVSampleFormat(AUDIO_FORMAT audio_fmt, AVSampleFormat& av_sample_fmt);
	static inline bool ConvertToAVChannelLayOut(CHANNEL_LAYOUT channel_lay_out, int64_t& av_chl_layout);
	static inline bool ConvertToAVPixelFormat(PIX_FMT pix_fmt, AVPixelFormat& av_pix_fmt);
	static inline uint32_t GetAudioChannels(CHANNEL_LAYOUT chl_layout);
	static inline size_t GetAudioPlanes(AUDIO_FORMAT audio_fmt, CHANNEL_LAYOUT chl_layout);
	static inline bool IsAudioPlanar(AUDIO_FORMAT audio_fmt);
}



#endif