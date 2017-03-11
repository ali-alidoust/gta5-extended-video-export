#pragma once

#pragma comment(lib, "lib\\ScriptHookV.lib")
#pragma comment(lib, "mfuuid.lib")

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