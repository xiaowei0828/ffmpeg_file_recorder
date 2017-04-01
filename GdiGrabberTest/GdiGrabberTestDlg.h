
// GdiGrabberTestDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include "VideoStatic.h"
#include <memory>

// CGdiGrabberTestDlg 对话框
class CGdiGrabberTestDlg : public CDialogEx
{
// 构造
public:
	CGdiGrabberTestDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CGdiGrabberTestDlg();
// 对话框数据
	enum { IDD = IDD_GDIGRABBERTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

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
	//MediaFileRecorder::IScreenAudioRecord* m_pRecorder;
	void* m_pRecorder;

	bool record_started_;
	bool record_interrupt_;
	CButton m_ButtonStart;
	int64_t start_capture_time_;

	int64_t duration_;
};
