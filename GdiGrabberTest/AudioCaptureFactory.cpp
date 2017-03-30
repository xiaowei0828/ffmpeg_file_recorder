
#include "stdafx.h"
#include "system_info.h"
#include "CWASAudioCapture.h"
#include "CWAVEAudioCapture.h"

namespace MediaFileRecorder
{
	IAudioCapture* CreateAudioCapture()
	{
		IAudioCapture* pAudioCapture = NULL;
		SystemInfo systemInfo;
		if (systemInfo.windows_version() >= SystemInfo::WINDOWS_VISTA)
		{
			pAudioCapture = new CWAVEAudioCapture();
		}
		else
		{
			pAudioCapture = new CWAVEAudioCapture();
		}
		return pAudioCapture;
	}


	void DestroyAudioCatpure(IAudioCapture* pAudioCapture)
	{
		delete pAudioCapture;
	}

}