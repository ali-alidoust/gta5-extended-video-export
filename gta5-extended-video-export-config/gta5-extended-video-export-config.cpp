#include "VoukoderTypeLib_h.h"

#include <Windows.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#define PRESET_FILE_NAME "preset.json"

void readEncoderConfig(VKENCODERCONFIG& out) {
    std::ifstream ifs(PRESET_FILE_NAME);
    //std::stringstream sts;
    //sts << ifs.rdbuf();

    nlohmann::json j = nlohmann::json::parse(ifs);

    VKENCODERCONFIG result{};

    result.version = j["version"];

    j["format"]["container"].get<std::string>().copy(result.format.container, sizeof(result.format.container));
    result.format.faststart = j["format"]["faststart"];

    j["video"]["encoder"].get<std::string>().copy(result.video.encoder, sizeof(result.video.encoder));
    j["video"]["options"].get<std::string>().copy(result.video.options, sizeof(result.video.options));
    j["video"]["filters"].get<std::string>().copy(result.video.filters, sizeof(result.video.filters));
    j["video"]["sidedata"].get<std::string>().copy(result.video.sidedata, sizeof(result.video.sidedata));

    j["audio"]["encoder"].get<std::string>().copy(result.audio.encoder, sizeof(result.audio.encoder));
    j["audio"]["options"].get<std::string>().copy(result.audio.options, sizeof(result.audio.options));
    j["audio"]["filters"].get<std::string>().copy(result.audio.filters, sizeof(result.audio.filters));
    j["audio"]["sidedata"].get<std::string>().copy(result.audio.sidedata, sizeof(result.audio.sidedata));

    out = result;
}

void writeEncoderConfig(const VKENCODERCONFIG& in) {
    nlohmann::json j;

    j["version"] = in.version;

    j["format"]["container"] = in.format.container;
    j["format"]["faststart"] = static_cast<bool>(in.format.faststart);

    j["video"]["encoder"] = in.video.encoder;
    j["video"]["options"] = in.video.options;
    j["video"]["filters"] = in.video.filters;
    j["video"]["sidedata"] = in.video.sidedata;

    j["audio"]["encoder"] = in.audio.encoder;
    j["audio"]["options"] = in.audio.options;
    j["audio"]["filters"] = in.audio.filters;
    j["audio"]["sidedata"] = in.audio.sidedata;

    std::ofstream ofs(PRESET_FILE_NAME);
    ofs << j.dump(4);
    ofs.flush();
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    HWND dummyHWND = ::CreateWindowA("STATIC", "Config", WS_VISIBLE, 0, 0, 10, 10, NULL, NULL, NULL, NULL);

    VKENCODERCONFIG vkConfig{};

    BOOL isOK = false;
    ShowCursor(TRUE);
    IVoukoder* pVoukoder = nullptr;
    // ACTCTX* actctx = nullptr;

    // ASSERT_RUNTIME(GetCurrentActCtx(reinterpret_cast<PHANDLE>(&actctx)),
    //"Failed to get Activation Context for current thread.");
    if (CoInitializeEx(nullptr, COINIT_MULTITHREADED) == S_OK) {
        if (CoCreateInstance(CLSID_CoVoukoder, nullptr, CLSCTX_INPROC_SERVER, IID_IVoukoder,
                             reinterpret_cast<void**>(&pVoukoder)) == S_OK) {

            try {
                readEncoderConfig(vkConfig);
                if (pVoukoder->SetConfig(vkConfig) != S_OK) {
                    throw std::exception();
                }
            } catch (std::exception&) {
                MessageBox(dummyHWND, "Failed to load configuration, using default values.", "Error", MB_ICONERROR);
                vkConfig = {.version = 1,
                            .video{.encoder{"libx264"},
                                   .options{"_pixelFormat=yuv420p|crf=17.000|opencl=1|preset=medium|rc=crf|"
                                            "x264-params=qpmax=22:aq-mode=2:aq-strength=0.700:rc-lookahead=180:"
                                            "keyint=480:min-keyint=3:bframes=11:b-adapt=2:ref=3:deblock=0:0:direct="
                                            "auto:me=umh:merange=32:subme=10:trellis=2:no-fast-pskip=1"},
                                   .filters{""},
                                   .sidedata{""}},
                            .audio{.encoder{"aac"},                                          // @clang-format
                                   .options{"_sampleFormat=fltp|b=320000|profile=aac_main"}, // @clang-format
                                   .filters{""},                                             // @clang-format
                                   .sidedata{""}},
                            .format = {.container{"mp4"}, .faststart = true}};
            }

            pVoukoder->ShowVoukoderDialog(true, true, &isOK, nullptr, GetModuleHandle(nullptr));

            if (isOK) {
                pVoukoder->GetConfig(&vkConfig);
                writeEncoderConfig(vkConfig);
                //     LL_NFO, "Updating configuration...");
                //     REQUIRE(pVoukoder->GetConfig(&vkConfig), "Failed to get config from Voukoder.");
                //     config::encoder_config = vkConfig;
                //     LOG_CALL(LL_DBG, config::writeEncoderConfig());

                //} else {
                //    LOG(LL_NFO, "Voukoder config cancelled by user.");
            }
        } else {
            MessageBox(dummyHWND, "Failed to initialize Voukoder. Make sure that it is installed.", "Error",
                       MB_ICONERROR);
        }
    } else {
        MessageBox(dummyHWND, "Failed to initialize COM.", "Error", MB_ICONERROR);
    }
}