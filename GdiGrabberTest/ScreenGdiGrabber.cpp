#include "stdafx.h"
#include "ScreenGdiGrabber.h"

CScreenGdiGrabber::CScreenGdiGrabber()
{
	frame_rate_ = 15;
	grab_rect_.left = grab_rect_.top = 0;
	grab_rect_.right = GetSystemMetrics(SM_CXSCREEN);
	grab_rect_.bottom = GetSystemMetrics(SM_CYSCREEN);
	started_ = false;
	last_tick_count_ = 0;
	perf_freq_.QuadPart = 0;
}

CScreenGdiGrabber::~CScreenGdiGrabber()
{
	if (started_)
	{
		StopGrab();
	}
}

void CScreenGdiGrabber::RegisterDataCb(IGdiGrabberDataCb* cb)
{
	vec_data_cb_.push_back(cb);
}

void CScreenGdiGrabber::UnRegisterDataCb(IGdiGrabberDataCb* cb)
{
	auto iter = std::find(vec_data_cb_.begin(), vec_data_cb_.end(), cb);
	if ( iter != vec_data_cb_.end())
	{
		vec_data_cb_.erase(iter);
	}
}

void CScreenGdiGrabber::SetGrabRect(const RECT& rect)
{
	grab_rect_ = rect;
}

void CScreenGdiGrabber::SetGrabFrameRate(int frame_rate)
{
	frame_rate_ = frame_rate;
}

bool CScreenGdiGrabber::StartGrab()
{
	if (started_)
	{
		OutputDebugStringA("Already started");
		return false;
	}

	CalculateFrameIntervalTick();

	src_dc_ = GetDC(NULL);
	dst_dc_ = CreateCompatibleDC(src_dc_);

	bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi_.bmiHeader.biWidth = grab_rect_.right - grab_rect_.left;
	bmi_.bmiHeader.biHeight = -(grab_rect_.bottom - grab_rect_.top);
	bmi_.bmiHeader.biPlanes = 1;
	bmi_.bmiHeader.biBitCount = 24;
	bmi_.bmiHeader.biCompression = BI_RGB;
	bmi_.bmiHeader.biSizeImage = 0;
	bmi_.bmiHeader.biXPelsPerMeter = 0;
	bmi_.bmiHeader.biYPelsPerMeter = 0;
	bmi_.bmiHeader.biClrUsed = 0;
	bmi_.bmiHeader.biClrImportant = 0;

	hbmp_ = CreateDIBSection(dst_dc_, &bmi_, DIB_RGB_COLORS, &bmp_buffer_, NULL, 0);
	if (!hbmp_)
	{
		OutputDebugStringA("Create DIB section failed");
		return false;
	}

	SelectObject(dst_dc_, hbmp_);
	StartGrabThread();
	started_ = true;
	return true;
}

void CScreenGdiGrabber::StopGrab()
{
	if (started_)
	{
		StopGrabThread();
		ReleaseDC(NULL, src_dc_);
		DeleteDC(dst_dc_);
		DeleteObject(hbmp_);
		started_ = false;
	}
}

void CScreenGdiGrabber::StartGrabThread()
{
	run_ = true;
	grab_thread_.swap(std::thread(std::bind(&CScreenGdiGrabber::GrabThreadProc, this)));
	SetThreadPriority(grab_thread_.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
}

void CScreenGdiGrabber::StopGrabThread()
{
	run_ = false;
	if (grab_thread_.joinable())
		grab_thread_.join();
}

void CScreenGdiGrabber::GrabThreadProc()
{
	while (run_)
	{
		int64_t interval_tick = GetCurrentTickCount() -last_tick_count_;
		while (interval_tick < frame_interval_tick_)
		{
			interval_tick = GetCurrentTickCount() -last_tick_count_;
		}
		last_tick_count_ = GetCurrentTickCount();
		int width = grab_rect_.right - grab_rect_.left;
		int heigth = grab_rect_.bottom - grab_rect_.top;
		BitBlt(dst_dc_, 0, 0, 
			width, heigth, src_dc_, 
			grab_rect_.left, grab_rect_.top,
			SRCCOPY);
		for (IGdiGrabberDataCb* cb : vec_data_cb_)
		{
			cb->OnScreenData(bmp_buffer_, width, heigth);
		}

		char log[128] = { 0 };
		_snprintf(log, 128, "required interval: %lld, interval: %lld\n",
			frame_interval_tick_ * 1000/perf_freq_.QuadPart,
			interval_tick*1000/perf_freq_.QuadPart);

		OutputDebugStringA(log);
	}
}

int64_t CScreenGdiGrabber::GetCurrentTickCount()
{
	LARGE_INTEGER current_counter;
	QueryPerformanceCounter(&current_counter);
	return current_counter.QuadPart;
}

void CScreenGdiGrabber::CalculateFrameIntervalTick()
{
	if (perf_freq_.QuadPart == 0)
	{
		QueryPerformanceFrequency(&perf_freq_);
	}

	frame_interval_tick_ = perf_freq_.QuadPart / frame_rate_;
}

