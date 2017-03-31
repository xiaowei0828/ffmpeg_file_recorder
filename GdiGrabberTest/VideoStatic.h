#pragma once
// CPenViewStatic
#include <afxwin.h>
#include "dib.h"
#include "IScreenGrabber.h"

class CVideoStatic : public CStatic, public MediaFileRecorder::IScreenGrabberDataCb
{
	DECLARE_DYNAMIC(CVideoStatic)

public:
	CVideoStatic();
	virtual ~CVideoStatic();
	// VideoDevice::IVideoFilter
	void OnScreenData(void* data, const MediaFileRecorder::VIDEO_INFO& videoInfo) override;

	DECLARE_MESSAGE_MAP()

protected:
	afx_msg void OnPaint();

private:
	CDib dib_;
	/*HDC mem_dc_;
	CRect rect_;*/
	uint32_t width_;
	uint32_t height_;
};


