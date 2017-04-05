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
            video_capture_rect.left = 500;
            video_capture_rect.top = 500;
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
                if ((ret & (int)MediaFileRecorder.RECORD_START_RESULT.STATE_NOT_RIGHT) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: state not right");
                else if((ret & (int)MediaFileRecorder.RECORD_START_RESULT.PARAMETER_INVALID) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: parameter invalid");
               else if((ret & (int)MediaFileRecorder.RECORD_START_RESULT.START_SCRREEN_CAPTURE_FAILED) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: start screen capture failed");
                else if((ret & (int)MediaFileRecorder.RECORD_START_RESULT.START_MIC_CAPTURE_FAILED) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: start mic capture failed");
                else if((ret & (int)MediaFileRecorder.RECORD_START_RESULT.START_SPEAKER_CAPTURE_FAILED) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: start speaker capture failed");
                else if((ret & (int)MediaFileRecorder.RECORD_START_RESULT.INIT_MEDIA_FILE_RECORDER_FAILED) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: init media file recorder failed");
                else if((ret & (int)MediaFileRecorder.RECORD_START_RESULT.START_MEDIA_FILE_RECORDER_FAILED) != 0)
                    System.Diagnostics.Debug.WriteLine("Start record: start media file recorder failed");


                Button s = (Button)sender;
                s.Text = "结束";
                m_bStarted = true;
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

        private void MR_LogCallback(MediaFileRecorder.SDK_LOG_LEVEL level, [MarshalAs(UnmanagedType.LPWStr)]string log)
        {
            if(level > MediaFileRecorder.SDK_LOG_LEVEL.LOG_DEBUG)
                System.Diagnostics.Debug.Print(log);
        }

    }
}
