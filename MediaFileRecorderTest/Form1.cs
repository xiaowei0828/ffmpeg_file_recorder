using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace MediaFileRecorderTest
{
    public partial class Form1 : Form
    {
        private IntPtr m_RecordObject;
        private MediaFileRecorder.RECORD_INFO m_stRecordInfo;
        private bool m_bStarted = false;
        private bool m_bSuspended = false;

        MediaFileRecorder.MR_LogCallback log_delegate;
        public Form1()
        {
            InitializeComponent();
            //int res = MediaFileRecorder.MR_Add(1, 2);
            log_delegate = new MediaFileRecorder.MR_LogCallback(this.MR_LogCallback);

            MediaFileRecorder.MR_SetLogCallBack(log_delegate);
           // GC.KeepAlive(log_delegate);

            m_RecordObject = MediaFileRecorder.MR_CreateScreenAudioRecorder();

            string fileName = "test.mp4";
            MediaFileRecorder.RECT video_capture_rect;
            video_capture_rect.left = 0;
            video_capture_rect.top = 0;
            video_capture_rect.right = 1920;
            video_capture_rect.bottom = 1080;
            m_stRecordInfo.file_name = new byte[1024];
            Array.Copy(System.Text.Encoding.UTF8.GetBytes(fileName), m_stRecordInfo.file_name, fileName.Length);
            m_stRecordInfo.is_record_mic = 1;
            m_stRecordInfo.is_record_speaker = 1;
            m_stRecordInfo.is_record_video = 1;
            m_stRecordInfo.video_capture_rect = video_capture_rect;
            m_stRecordInfo.video_frame_rate = 20;
            m_stRecordInfo.quality = MediaFileRecorder.QUALITY.HIGH;
            m_stRecordInfo.video_dst_width = 1920;
            m_stRecordInfo.video_dst_height = 1080;

            int ret = MediaFileRecorder.MR_SetRecordInfo(m_RecordObject, ref m_stRecordInfo);
        }

        private void button_start_MouseClick(object sender, MouseEventArgs e)
        {
            if(!m_bStarted)
            {
               int ret = MediaFileRecorder.MR_StartRecord(m_RecordObject);
                if(ret == 0)
                {
                    Button s = (Button)sender;
                    s.Text = "结束";
                    m_bStarted = true;
                }
            }
            else
            {
                int ret = MediaFileRecorder.MR_StopRecord(m_RecordObject);
                if(ret == 0)
                {
                    Button s = (Button)sender;
                    s.Text = "开始";
                    m_bStarted = false;
                }
            }
        }

        private void button_suspend_MouseClick(object sender, MouseEventArgs e)
        {
            if (!m_bSuspended)
            {
                int ret = MediaFileRecorder.MR_SuspendRecord(m_RecordObject);
                if (ret == 0)
                {
                    Button s = (Button)sender;
                    s.Text = "继续";
                    m_bSuspended = true;
                }
            }
            else
            {
                int ret = MediaFileRecorder.MR_ResumeRecord(m_RecordObject);
                if (ret == 0)
                {
                    Button s = (Button)sender;
                    s.Text = "中断";
                    m_bSuspended = false;
                }
            }
        }

        private void MR_LogCallback(int level, [MarshalAs(UnmanagedType.LPWStr)]string log)
        {
            System.Diagnostics.Debug.Print(log);
        }

    }
}
