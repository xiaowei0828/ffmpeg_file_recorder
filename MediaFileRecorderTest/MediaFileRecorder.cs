using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace MediaFileRecorderTest
{
    class MediaFileRecorder
    {
        public struct RECT
        {
           public int left;
           public int right;
           public int top;
           public int bottom;
        }

        public enum QUALITY
        {
            UNKOWN = 0,
            NORMAL,
            HIGH,
            VERY_HIGH
        }
        public struct RECORD_INFO
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1024)] 
            public byte[] file_name;
            public int is_record_video;
            public int is_record_mic;
            public int is_record_speaker;
            public RECT video_capture_rect;
            public int video_dst_width;
            public int video_dst_height;
            public int video_frame_rate;
            public QUALITY quality;
        }

        public enum RECORD_START_RESULT
        {
            STATE_NOT_RIGHT = 0x1,
            PARAMETER_INVALID = 0x2,
            START_SCRREEN_CAPTURE_FAILED = 0x4,
            START_MIC_CAPTURE_FAILED = 0x8,
            START_SPEAKER_CAPTURE_FAILED = 0x10,
            INIT_MEDIA_FILE_RECORDER_FAILED = 0x20,
            START_MEDIA_FILE_RECORDER_FAILED = 0x40
        }

        public enum SDK_LOG_LEVEL
        {
            LOG_DEBUG = 0,
            LOG_INFO,
            LOG_WARNING,
            LOG_ERROR
        };

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void MR_LogCallback(SDK_LOG_LEVEL level, [MarshalAs(UnmanagedType.LPWStr)]string log);
        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_SetLogCallBack", CallingConvention = CallingConvention.Cdecl)]
        public static extern void MR_SetLogCallBack(MR_LogCallback cb);

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_CreateScreenAudioRecorder", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr MR_CreateScreenAudioRecorder();

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_DestroyScreenAudioRecorder", CallingConvention = CallingConvention.Cdecl)]
        public static extern void MR_DestroyScreenAudioRecorder(IntPtr pObject);

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_SetRecordInfo", CallingConvention = CallingConvention.Cdecl)]
        public static extern int MR_SetRecordInfo(IntPtr pObject, ref RECORD_INFO recordInfo);

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_StartRecord", CallingConvention = CallingConvention.Cdecl)]
        public static extern int MR_StartRecord(IntPtr pObject);

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_SuspendRecord", CallingConvention = CallingConvention.Cdecl)]
        public static extern int MR_SuspendRecord(IntPtr pObject);

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_ResumeRecord", CallingConvention = CallingConvention.Cdecl)]
        public static extern int MR_ResumeRecord(IntPtr pObject);

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_StopRecord", CallingConvention = CallingConvention.Cdecl)]
        public static extern int MR_StopRecord(IntPtr pObject);
        
    }
}
