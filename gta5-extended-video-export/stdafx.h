#pragma once

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


extern "C" {
#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
#include "libavutil\audio_fifo.h"
#include <yara\libyara.h>
#include <yara\rules.h>
#include <yara\compiler.h>
}