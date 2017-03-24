#ifndef  SCREENGDIGRABBER_H
#define  SCREENGDIGRABBER_H

#include <vector>
#include <windows.h>
#include <atomic>
#include <thread>

class IGdiGrabberDataCb
{
public:
	virtual ~IGdiGrabberDataCb(){};

	// RGB data callback
	virtual void OnScreenData(void* data, int width, int height) = 0;
};

class CScreenGdiGrabber
{
public:
	CScreenGdiGrabber();
	~CScreenGdiGrabber();

	void RegisterDataCb(IGdiGrabberDataCb* cb);
	void UnRegisterDataCb(IGdiGrabberDataCb* cb);

	void SetGrabRect(const RECT& rect);
	void SetGrabFrameRate(int frame_rate);

	bool StartGrab();
	void StopGrab();

private:
	void StartGrabThread();
	void StopGrabThread();
	void GrabThreadProc();

	void CalculateFrameIntervalTick();
	int64_t GetCurrentTickCount();

private:
	std::vector<IGdiGrabberDataCb*> vec_data_cb_;
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

#endif