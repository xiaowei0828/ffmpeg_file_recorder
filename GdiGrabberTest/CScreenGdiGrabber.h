#ifndef  SCREENGDIGRABBER_H
#define  SCREENGDIGRABBER_H

#include "IScreenGrabber.h"
#include <vector>
#include <windows.h>
#include <atomic>
#include <thread>

namespace MediaFileRecorder
{
	class CScreenGdiGrabber : public IScreenGrabber
	{
	public:
		CScreenGdiGrabber();
		~CScreenGdiGrabber();

		void RegisterDataCb(IScreenGrabberDataCb* cb) override;
		void UnRegisterDataCb(IScreenGrabberDataCb* cb) override;

		void SetGrabRect(int left, int top, int right, int bottom) override;
		void SetGrabFrameRate(int frame_rate) override;

		bool StartGrab() override;
		void StopGrab() override;

	private:
		void StartGrabThread();
		void StopGrabThread();
		void GrabThreadProc();

		void CalculateFrameIntervalTick();
		int64_t GetCurrentTickCount();

	private:
		std::vector<IScreenGrabberDataCb*> vec_data_cb_;
		RECT grab_rect_;
		int frame_rate_;

		bool started_;
		std::atomic_bool run_;
		HDC src_dc_;
		HDC dst_dc_;
		BITMAPINFO bmi_;
		HBITMAP hbmp_;
		void* bmp_buffer_;
		std::thread grab_thread_;

		LARGE_INTEGER perf_freq_;
		int64_t last_tick_count_;
		int64_t frame_interval_tick_;

		HANDLE m_hGrabTimer;
	};
}


#endif