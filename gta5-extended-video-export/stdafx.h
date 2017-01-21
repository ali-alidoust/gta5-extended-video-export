#pragma once

#pragma comment(lib, "delayimp")  

#pragma comment(lib, "lib\\ScriptHookV.lib")

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "IlmImf.lib")
#pragma comment(lib, "Half.lib")
#pragma comment(lib, "IlmThread.lib")
#pragma comment(lib, "Iex.lib")
#pragma comment(lib, "zlibstatic.lib")
#pragma comment(lib, "PolyHook.lib")
//#pragma comment(lib, "x3daudio.lib")
//#pragma comment(lib, "xapofx.lib")

#include <sstream>

#include "inc\main.h"
#include "inc\nativeCaller.h"
#include "inc\natives.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <string>
#include <regex>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include <wrl.h>

#include "config.h"

#include <ImfImage.h>

extern "C" {
#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
#include "libavutil\audio_fifo.h"
#include "libavutil\opt.h"
#include <yara\libyara.h>
#include <yara\rules.h>
#include <yara\compiler.h>
}