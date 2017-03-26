
// GdiGrabberTestDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include "VideoStatic.h"
#include "IScreenGrabber.h"
#include "CMediaFileRecorder.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include <memory>
#include "webrtc/system_wrappers/interface/trace.h"

// CGdiGrabberTestDlg 对话框
class CGdiGrabberTestDlg : public CDialogEx, 
	public ScreenGrabber::IScreenGrabberDataCb,
	public webrtc::AudioTransport, 
	public webrtc::AudioDeviceObserver,
	public webrtc::TraceCallback
{
// 构造
public:
	CGdiGrabberTestDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_GDIGRABBERTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

	void OnScreenData(void* data, int width, int height, ScreenGrabber::PIX_FMT pix_fmt) override;

public:
	/*webrtc::AudioTransport*/
	int32_t RecordedDataIsAvailable(const void* audioSamples,
		const size_t nSamples,
		const size_t nBytesPerSample,
		const uint8_t nChannels,
		const uint32_t samplesPerSec,
		const uint32_t totalDelayMS,
		const int32_t clockDrift,
		const uint32_t currentMicLevel,
		const bool keyPressed,
		uint32_t& newMicLevel) override;

	int32_t NeedMorePlayData(const size_t nSamples,
		const size_t nBytesPerSample,
		const uint8_t nChannels,
		const uint32_t samplesPerSec,
		void* audioSamples,
		size_t& nSamplesOut,
		int64_t* elapsed_time_ms,
		int64_t* ntp_time_ms) override 
	{
		return 0;
	};

	/*added by wrb, for capturing playout data*/
	int32_t RecordedPlayDataIsAvailable(const void* audioSamples,
		const size_t nSamples,
		const size_t nBytesPerSample,
		const uint8_t nChannels,
		const uint32_t samplesPerSec,
		const uint32_t totalDelayMS,
		const int32_t clockDrift) override;

	/*webrtc::AudioDeviceObserver*/
	void OnErrorIsReported(const ErrorCode error) override;
	void OnWarningIsReported(const WarningCode warning) override;

	/*webrtc::TraceCallback*/
	void Print(webrtc::TraceLevel level, const char* message, int length);

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnClose();
	afx_msg void OnBnClickedButtonInterrupt();
	DECLARE_MESSAGE_MAP()
private:
	CVideoStatic m_StaticPic;
	std::shared_ptr<ScreenGrabber::IScreenGrabber> screen_grabber_;
	std::shared_ptr<CMediaFileRecorder> media_file_recorder_;

	webrtc::AudioDeviceModule* audio_dev_module_;
	bool record_started_;
	bool record_interrupt_;
	CButton m_ButtonStart;
	int64_t start_capture_time_;

	int64_t duration_;
};
