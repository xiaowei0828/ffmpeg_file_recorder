
// GdiGrabberTestDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "GdiGrabberTest.h"
#include "GdiGrabberTestDlg.h"
#include "afxdialogex.h"
#include <timeapi.h>

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
	record_started_ = false;
	record_interrupt_ = false;

	webrtc::Trace::CreateTrace();
	webrtc::Trace::set_level_filter(webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical);
	webrtc::Trace::SetTraceCallback(this);
}

void CGdiGrabberTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_PIC, m_StaticPic);
	DDX_Control(pDX, IDC_BUTTON_START, m_ButtonStart);
	webrtc::Trace::ReturnTrace();
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
#define CAPTURE_FRAME_RATE 15

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
	
	
	gdi_grabber_.reset(new CScreenGdiGrabber());

	media_file_recorder_.reset(new CMediaFileRecorder());
	
	audio_dev_module_ = webrtc::CreateAudioDeviceModule(0, webrtc::AudioDeviceModule::AudioLayer::kPlatformDefaultAudio);
	audio_dev_module_->AddRef();
	audio_dev_module_->Init();
	audio_dev_module_->RegisterAudioCallback(this);
	audio_dev_module_->RegisterEventObserver(this);
	audio_dev_module_->SetRecordingDevice(webrtc::AudioDeviceModule::kDefaultDevice);
	audio_dev_module_->SetPlayoutDevice(webrtc::AudioDeviceModule::kDefaultDevice);

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
		RECT grab_rect = { CAPTURE_LEFT, CAPTURE_TOP, CAPTURE_WIDTH, CAPTURE_HEIGHT };
		gdi_grabber_->SetGrabFrameRate(CAPTURE_FRAME_RATE);
		gdi_grabber_->SetGrabRect(grab_rect);
		gdi_grabber_->RegisterDataCb(this);
		gdi_grabber_->RegisterDataCb(&m_StaticPic);

		CMediaFileRecorder::RecordInfo record_info;
		strcpy(record_info.file_name, "test.mp4");
		record_info.video_info.src_width = CAPTURE_WIDTH;
		record_info.video_info.src_height = CAPTURE_HEIGHT;
		record_info.video_info.dst_width = CAPTURE_WIDTH;
		record_info.video_info.dst_height = CAPTURE_HEIGHT;
		record_info.video_info.frame_rate = CAPTURE_FRAME_RATE;
		record_info.video_info.src_pix_fmt = AV_PIX_FMT_RGB24;
		media_file_recorder_->Init(record_info);

		gdi_grabber_->StartGrab();

		int32_t ret;
		//ret = audio_dev_module_->InitRecording();
		//ret = audio_dev_module_->StartRecording();
		//audio_dev_module_->SetStereoPlayout(false);
		ret = audio_dev_module_->InitCapturePlayout();
		ret = audio_dev_module_->StartCapturePlayout();

		record_started_ = true;
		m_ButtonStart.SetWindowTextW(L"停止");
		media_file_recorder_->Start();
		start_capture_time_ = timeGetTime();
		duration_ = 0;
	}
	else
	{
		record_started_ = false;
		gdi_grabber_->StopGrab();
		//audio_dev_module_->StopRecording();
		audio_dev_module_->StopCapturePlayout();
		media_file_recorder_->Stop();
		media_file_recorder_->UnInit();
		m_ButtonStart.SetWindowTextW(L"开始");

		gdi_grabber_->UnRegisterDataCb(this);
		gdi_grabber_->UnRegisterDataCb(&m_StaticPic);

		if (!record_interrupt_)
			duration_ += timeGetTime() - start_capture_time_;
		
		char log[128] = { 0 };
		_snprintf(log, 128, "duration: %lld \n", duration_);
		OutputDebugStringA(log);
	}
		
}


void CGdiGrabberTestDlg::OnClose()
{
	// TODO:  在此添加消息处理程序代码和/或调用默认值
	audio_dev_module_->Terminate();
	audio_dev_module_->Release();
	CDialogEx::OnClose();
}

void CGdiGrabberTestDlg::OnScreenData(void* data, int width, int height)
{
	if (media_file_recorder_)
	{
		media_file_recorder_->FillVideo(data);
	}
}


void CGdiGrabberTestDlg::OnBnClickedButtonInterrupt()
{
	// TODO:  在此添加控件通知处理程序代码
	if (record_started_)
	{
		if (!record_interrupt_)
		{
			gdi_grabber_->StopGrab();
			audio_dev_module_->StopRecording();
			media_file_recorder_->Stop();
			record_interrupt_ = true;
			GetDlgItem(IDC_BUTTON_INTERRUPT)->SetWindowTextW(L"继续");

			duration_ += timeGetTime() - start_capture_time_;
			char log[128] = { 0 };
			_snprintf(log, 128, "duration: %lld \n", duration_);
			OutputDebugStringA(log);
		}
		else
		{
			gdi_grabber_->StartGrab();
			audio_dev_module_->InitRecording();
			audio_dev_module_->StartRecording();
			media_file_recorder_->Start();
			start_capture_time_ = timeGetTime();
			record_interrupt_ = false;
			GetDlgItem(IDC_BUTTON_INTERRUPT)->SetWindowTextW(L"暂停");
		}
	}
}

int32_t CGdiGrabberTestDlg::RecordedDataIsAvailable(const void* audioSamples, const size_t nSamples, 
	const size_t nBytesPerSample, const uint8_t nChannels, 
	const uint32_t samplesPerSec, const uint32_t totalDelayMS, 
	const int32_t clockDrift, const uint32_t currentMicLevel, 
	const bool keyPressed, uint32_t& newMicLevel)
{
	if (media_file_recorder_)
	{
		int64_t chl_layout;
		if (nChannels == 1)
			chl_layout = AV_CH_LAYOUT_MONO;
		else if (nChannels == 2)
			chl_layout = AV_CH_LAYOUT_STEREO;
		else
		{
			OutputDebugStringA("Invalid channel type");
			return 0;
		}

		media_file_recorder_->FillAudio(audioSamples, nSamples, 
			samplesPerSec, chl_layout, 
			AVSampleFormat::AV_SAMPLE_FMT_S16);
	}
	return 0;
}

int32_t CGdiGrabberTestDlg::RecordedPlayDataIsAvailable(const void* audioSamples, const size_t nSamples, 
	const size_t nBytesPerSample, const uint8_t nChannels, 
	const uint32_t samplesPerSec, const uint32_t totalDelayMS, 
	const int32_t clockDrift)
{
	if (media_file_recorder_)
	{
		int64_t chl_layout;
		if (nChannels == 1)
			chl_layout = AV_CH_LAYOUT_MONO;
		else if (nChannels == 2)
			chl_layout = AV_CH_LAYOUT_STEREO;
		else
		{
			OutputDebugStringA("Invalid channel type");
			return 0;
		}

		media_file_recorder_->FillAudio(audioSamples, nSamples,
			samplesPerSec, chl_layout,
			AVSampleFormat::AV_SAMPLE_FMT_S16);
	}
	return 0;
}

void CGdiGrabberTestDlg::OnErrorIsReported(const ErrorCode error)
{
	return;
}

void CGdiGrabberTestDlg::OnWarningIsReported(const WarningCode warning)
{

}

void CGdiGrabberTestDlg::Print(webrtc::TraceLevel level, const char* message, int length)
{
	std::string log(message, length);
	OutputDebugStringA(log.c_str());
}
