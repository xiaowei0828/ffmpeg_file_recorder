
// GdiGrabberTestDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include "VideoStatic.h"
#include "ScreenGdiGrabber.h"
#include "MediaFileRecorder.h"
#include <memory>

// CGdiGrabberTestDlg 对话框
class CGdiGrabberTestDlg : public CDialogEx, IGdiGrabberDataCb
{
// 构造
public:
	CGdiGrabberTestDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_GDIGRABBERTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

	void OnScreenData(void* data, int width, int height) override;


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
	DECLARE_MESSAGE_MAP()
private:
	CVideoStatic m_StaticPic;
	std::shared_ptr<CScreenGdiGrabber> gdi_grabber_;
	std::shared_ptr<CMediaFileRecorder> media_file_recorder_;
	bool capture_started_;
	CButton m_ButtonStart;
public:
	afx_msg void OnBnClickedButtonFinish();
};
