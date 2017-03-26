#ifndef ISCREENGRABBER_H
#define ISCREENGRABBER_H

namespace ScreenGrabber {

	enum PIX_FMT
	{
		PIX_FMT_RGB24,
		PIX_FMT_BGR24,
		PIX_FMT_ARGB,
		PIX_FMT_BGRA,
	};

	class IScreenGrabberDataCb
	{
	public:
		virtual ~IScreenGrabberDataCb(){};

		// RGB data callback
		virtual void OnScreenData(void* data, int width, int height, PIX_FMT pix_fmt) = 0;
	};

	class IScreenGrabber
	{
	public:
		virtual void RegisterDataCb(IScreenGrabberDataCb* cb) = 0;
		virtual void UnRegisterDataCb(IScreenGrabberDataCb* cb) = 0;

		virtual void SetGrabRect(int left, int top, int right, int bottom) = 0;
		virtual void SetGrabFrameRate(int frame_rate) = 0;

		virtual bool StartGrab() = 0;
		virtual void StopGrab() = 0;

	protected:
		virtual ~IScreenGrabber(){}
	};
}
#endif