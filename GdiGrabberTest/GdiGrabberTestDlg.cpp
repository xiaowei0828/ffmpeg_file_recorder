
// GdiGrabberTestDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "GdiGrabberTest.h"
#include "GdiGrabberTestDlg.h"
#include "afxdialogex.h"

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



CGdiGrabberTestDlg::CGdiGrabberTestDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CGdiGrabberTestDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	capture_started_ = false;
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
	ON_BN_CLICKED(IDC_BUTTON_FINISH, &CGdiGrabberTestDlg::OnBnClickedButtonFinish)
END_MESSAGE_MAP()


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

	RECT grab_rect = {0, 0, 600, 400};
	gdi_grabber_.reset(new CScreenGdiGrabber());
	gdi_grabber_->RegisterDataCb(&m_StaticPic);
	gdi_grabber_->SetGrabFrameRate(20);
	gdi_grabber_->SetGrabRect(grab_rect);

	gdi_grabber_->RegisterDataCb(this);

	media_file_recorder_.reset(new CMediaFileRecorder());
	if (media_file_recorder_)
	{
		CMediaFileRecorder::RecordInfo record_info;
		strcpy(record_info.file_name, "test.mp4");
		record_info.video_info.src_width = 600;
		record_info.video_info.src_height = 400;
		record_info.video_info.dst_width = 600;
		record_info.video_info.dst_height = 400;
		record_info.video_info.frame_rate = 20;
		record_info.video_info.bit_rate = 400000;
		record_info.video_info.src_pix_fmt = AV_PIX_FMT_RGB24;
		media_file_recorder_->Init(record_info);
	}


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
	if (gdi_grabber_)
	{
		if (!capture_started_)
		{
			if (gdi_grabber_->StartGrab())
			{
				capture_started_ = true;
				m_ButtonStart.SetWindowTextW(L"停止");
			}
			if (media_file_recorder_)
				media_file_recorder_->Start();
		}
		else
		{
			media_file_recorder_->Stop();
			gdi_grabber_->StopGrab();
			m_ButtonStart.SetWindowTextW(L"开始");
			capture_started_ = false;
		}
		
	}
}


void CGdiGrabberTestDlg::OnClose()
{
	// TODO:  在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnClose();
}

void CGdiGrabberTestDlg::OnScreenData(void* data, int width, int height)
{
	if (media_file_recorder_)
	{
		media_file_recorder_->FillVideo(data);
	}
}


void CGdiGrabberTestDlg::OnBnClickedButtonFinish()
{
	// TODO:  在此添加控件通知处理程序代码
	if (media_file_recorder_)
		media_file_recorder_->UnInit();
}
