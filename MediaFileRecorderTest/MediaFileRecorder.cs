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

        [DllImport("screen_audio_recorder.dll", EntryPoint = "MR_Add", CallingConvention = CallingConvention.Cdecl)]
        public static extern int MR_Add(int a, int b);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void MR_LogCallback(int level, [MarshalAs(UnmanagedType.LPWStr)]string log);
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
