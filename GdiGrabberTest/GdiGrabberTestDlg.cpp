
// GdiGrabberTestDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "GdiGrabberTest.h"
#include "GdiGrabberTestDlg.h"
#include "afxdialogex.h"
#include <MMSystem.h>
#include "../screen_audio_recorder/IScreenAudioRecord_C.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CGdiGrabberTestDlg 对话框

void recorder_log_cb(MediaFileRecorder::SDK_LOG_LEVEL level, const wchar_t* msg)
{
	if (level >= MediaFileRecorder::LOG_INFO)
	{
		OutputDebugString(msg);
		OutputDebugString(L"\n");
	}
	
}

CGdiGrabberTestDlg::CGdiGrabberTestDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CGdiGrabberTestDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	record_started_ = false;
	record_interrupt_ = false;
	
}


CGdiGrabberTestDlg::~CGdiGrabberTestDlg()
{
}


void CGdiGrabberTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_PIC, m_StaticPic);
	DDX_Control(pDX, IDC_BUTTON_START, m_ButtonStart);
}

BEGIN_MESSAGE_MAP(CGdiGrabberTestDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_START, &CGdiGrabberTestDlg::OnBnClickedButtonStart)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON_INTERRUPT, &CGdiGrabberTestDlg::OnBnClickedButtonInterrupt)
END_MESSAGE_MAP()


#define CAPTURE_LEFT 0
#define CAPTURE_TOP 0
#define CAPTURE_WIDTH 1920
#define CAPTURE_HEIGHT 1080
#define CAPTURE_FRAME_RATE 20

// CGdiGrabberTestDlg 消息处理程序

BOOL CGdiGrabberTestDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO:  在此添加额外的初始化代码

	//m_pRecorder = MediaFileRecorder::CreateScreenAudioRecorder();
	//MediaFileRecorder::SetLogCallback(recorder_log_cb);
	m_pRecorder = MR_CreateScreenAudioRecorder();
	MR_SetLogCallBack(recorder_log_cb);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CGdiGrabberTestDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CGdiGrabberTestDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CGdiGrabberTestDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CGdiGrabberTestDlg::OnBnClickedButtonStart()
{
	// TODO:  在此添加控件通知处理程序代码

	if (!record_started_)
	{
		MediaFileRecorder::RECT grab_rect;
		grab_rect.left = CAPTURE_LEFT;
		grab_rect.top = CAPTURE_TOP;
		grab_rect.right = CAPTURE_LEFT + CAPTURE_WIDTH;
		grab_rect.bottom = CAPTURE_TOP + CAPTURE_HEIGHT;

		MediaFileRecorder::RECORD_INFO record_info;
		
		strcpy_s(record_info.file_name, "test.mp4");
		record_info.video_capture_rect = grab_rect;
		record_info.video_dst_width = 1920;
		record_info.video_dst_height = 1080;
		record_info.video_frame_rate= CAPTURE_FRAME_RATE;
		record_info.quality = MediaFileRecorder::NORMAL;
		record_info.is_record_speaker = true;
		record_info.is_record_video = true;
		record_info.is_record_mic = true;

		/*int ret = m_pRecorder->SetRecordInfo(record_info);
		ret = m_pRecorder->StartRecord();*/
		int ret = MR_SetRecordInfo(m_pRecorder, record_info);
		ret = MR_StartRecord(m_pRecorder);

		record_started_ = true;
		m_ButtonStart.SetWindowTextW(L"停止");
		start_capture_time_ = timeGetTime();
		duration_ = 0;
	}
	else
	{
		record_started_ = false;
		MR_StopRecord(m_pRecorder);
		m_ButtonStart.SetWindowTextW(L"开始");

		if (!record_interrupt_)
			duration_ += timeGetTime() - start_capture_time_;
		
		char log[128] = { 0 };
		_snprintf_s(log, 128, "duration: %lld \n", duration_);
		OutputDebugStringA(log);
	}
		
}


void CGdiGrabberTestDlg::OnClose()
{
	// TODO:  在此添加消息处理程序代码和/或调用默认
	MR_DestroyScreenAudioRecorder(m_pRecorder);
	CDialogEx::OnClose();
}


void CGdiGrabberTestDlg::OnBnClickedButtonInterrupt()
{
	// TODO:  在此添加控件通知处理程序代码
	if (record_started_)
	{
		if (!record_interrupt_)
		{
			MR_SuspendRecord(m_pRecorder);
			record_interrupt_ = true;
			GetDlgItem(IDC_BUTTON_INTERRUPT)->SetWindowTextW(L"继续");

			duration_ += timeGetTime() - start_capture_time_;
			char log[128] = { 0 };
			_snprintf_s(log, 128, "duration: %lld \n", duration_);
			OutputDebugStringA(log);
		}
		else
		{
			MR_ResumeRecord(m_pRecorder);
			start_capture_time_ = timeGetTime();
			record_interrupt_ = false;
			GetDlgItem(IDC_BUTTON_INTERRUPT)->SetWindowTextW(L"暂停");
		}
	}
}
