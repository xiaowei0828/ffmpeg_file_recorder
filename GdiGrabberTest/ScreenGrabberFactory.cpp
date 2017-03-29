
#include "stdafx.h"
#include "IScreenGrabber.h"
#include "CScreenGdiGrabber.h"
#include "CScreenDXGrabber.h"
#include "system_info.h"

namespace MediaFileRecorder
{
	IScreenGrabber* CreateScreenGrabber()
	{
		SystemInfo systemInfo;
		if (systemInfo.windows_version() >= SystemInfo::WINDOWS_8)
		{
			return NULL;
		}
		else
		{
			IScreenGrabber* pScreenGrabber = new CScreenGdiGrabber();
			return pScreenGrabber;
		}

	}
}