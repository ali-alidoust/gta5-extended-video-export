// script.cpp : Defines the exported functions for the DLL application.
//

#include "script.h"
#include "MFUtility.h"
#include "encoder.h"
#include "hooks.h"
#include "logger.h"
#include "stdafx.h"
#include "util.h"
#include "yara-helper.h"
// #include <DirectXMath.h>
#include <mferror.h>

// #include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\comdecl.h>
// #include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2.h>
// #include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\XAudio2fx.h>
// #include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\XAPOFX.h>
#pragma warning(push)
#pragma warning(disable : 4005)
// #include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\x3daudio.h>
#pragma warning(pop)
// #pragma comment(lib,"x3daudio.lib")
// #pragma comment(lib,"xapofx.lib")

#include <DirectXTex.h>
#include <variant>

namespace PSAccumulate {
#include <PSAccumulate.h>
}

namespace PSDivide {
#include <PSDivide.h>
}

namespace VSFullScreen {
#include <VSFullScreen.h>
}

using namespace Microsoft::WRL;
using namespace DirectX;

namespace {
// std::shared_ptr<PLH::VFuncSwapHook> hkIMFSinkWriter_AddStream();
// std::shared_ptr<PLH::VFuncSwapHook> hkIMFSinkWriter_SetInputMediaType();
// std::shared_ptr<PLH::VFuncSwapHook> hkIMFSinkWriter_WriteSample();
// std::shared_ptr<PLH::VFuncSwapHook> hkIMFSinkWriter_Finalize();
// std::shared_ptr<PLH::VFuncSwapHook> hkOMSetRenderTargets();
// std::shared_ptr<PLH::VFuncSwapHook> hkDraw();
// std::shared_ptr<PLH::VFuncSwapHook> hkCreateSourceVoice();
// std::shared_ptr<PLH::VFuncSwapHook> hkSubmitSourceBuffer();
//
// std::shared_ptr<PLH::IatHook> hkCoCreateInstance();
// std::shared_ptr<PLH::IatHook> hkMFCreateSinkWriterFromURL();
//
// std::shared_ptr<PLH::x64Detour> hkGetFrameRateFraction();
// std::shared_ptr<PLH::x64Detour> hkGetRenderTimeBase();
// std::shared_ptr<PLH::x64Detour> hkStepAudio();
// std::shared_ptr<PLH::x64Detour> hkGetGameSpeedMultiplier();
// std::shared_ptr<PLH::x64Detour> hkCreateThread();
// std::shared_ptr<PLH::x64Detour> hkGetFrameRate();
// std::shared_ptr<PLH::x64Detour> hkGetAudioSamples();
// std::shared_ptr<PLH::x64Detour> hkUnk01();
// std::shared_ptr<PLH::x64Detour> hkCreateTexture();
// std::shared_ptr<PLH::x64Detour> hkCreateExportTexture();
// std::shared_ptr<PLH::x64Detour> hkLinearizeTexture();
// std::shared_ptr<PLH::x64Detour> hkAudioUnk01();
// std::shared_ptr<PLH::x64Detour> hkWaitForSingleObject();

/*std::shared_ptr<PLH::X64Detour> hkGetGlobalVariableIndex();
std::shared_ptr<PLH::X64Detour> hkGetVariable();
std::shared_ptr<PLH::X64Detour> hkGetMatrices();
std::shared_ptr<PLH::X64Detour> hkGetVar();
std::shared_ptr<PLH::X64Detour> hkGetVarPtrByHash();*/
ComPtr<ID3D11Texture2D> pGameBackBuffer;
ComPtr<ID3D11Texture2D> pGameBackBufferResolved;
ComPtr<ID3D11Texture2D> pGameDepthBufferQuarterLinear;
ComPtr<ID3D11Texture2D> pGameDepthBufferQuarter;
ComPtr<ID3D11Texture2D> pGameDepthBufferResolved;
ComPtr<ID3D11Texture2D> pGameDepthBuffer;
ComPtr<ID3D11Texture2D> pGameGBuffer0;
ComPtr<ID3D11Texture2D> pGameEdgeCopy;
ComPtr<ID3D11Texture2D> pLinearDepthTexture;
ComPtr<ID3D11Texture2D> pStencilTexture;

DWORD threadIdRageAudioMixThread = 0;

void* pCtxLinearizeBuffer = NULL;
// std::shared_ptr<PLH::X64Detour> hkGetTexture();
bool isCustomFrameRateSupported = false;

std::unique_ptr<Encoder::Session> encodingSession;
std::mutex mxSession;

void* pGlobalUnk01 = NULL;

std::thread::id mainThreadId;
ComPtr<IDXGISwapChain> mainSwapChain;

ComPtr<IDXGIFactory> pDXGIFactory;

std::queue<std::packaged_task<void()>> asyncQueue;
std::mutex mxAsyncQueue;

ComPtr<ID3D11DeviceContext> pDContext;

ComPtr<ID3D11Texture2D> pMotionBlurAccBuffer;
ComPtr<ID3D11Texture2D> pMotionBlurFinalBuffer;
ComPtr<ID3D11VertexShader> pVSFullScreen;
ComPtr<ID3D11PixelShader> pPSAccumulate;
ComPtr<ID3D11PixelShader> pPSDivide;
ComPtr<ID3D11Buffer> pDivideConstantBuffer;
ComPtr<ID3D11RasterizerState> pRasterState;
ComPtr<ID3D11SamplerState> pPSSampler;
ComPtr<ID3D11RenderTargetView> pRTVAccBuffer;
ComPtr<ID3D11RenderTargetView> pRTVBlurBuffer;
ComPtr<ID3D11ShaderResourceView> pSRVAccBuffer;
ComPtr<ID3D11Texture2D> pSourceSRVTexture;
ComPtr<ID3D11ShaderResourceView> pSourceSRV;
ComPtr<ID3D11BlendState> pAccBlendState;

struct PSConstantBuffer {
    XMFLOAT4 floats;
} cb;

struct ExportContext {

    ExportContext() {
        PRE();
        POST();
    }

    ~ExportContext() {
        PRE();
        POST();
    }

    float audioSkipCounter = 0;
    float audioSkip = 0;

    bool isAudioExportDisabled = false;

    bool captureRenderTargetViewReference = false;
    bool captureDepthStencilViewReference = false;

    ComPtr<ID3D11Texture2D> pExportRenderTarget;
    ComPtr<ID3D11DeviceContext> pDeviceContext;
    ComPtr<ID3D11Device> pDevice;
    ComPtr<IDXGIFactory> pDXGIFactory;
    ComPtr<IDXGISwapChain> pSwapChain;

    float far_clip = 0;
    float near_clip = 0;

    std::shared_ptr<DirectX::ScratchImage> capturedImage = std::make_shared<DirectX::ScratchImage>();

    UINT pts = 0;

    ComPtr<IMFMediaType> videoMediaType;

    int accCount = 0;
    int totalFrameNum = 0;
};

std::shared_ptr<ExportContext> exportContext;

std::shared_ptr<YaraHelper> pYaraHelper;

} // namespace

// tCoCreateInstance oCoCreateInstance;
// tMFCreateSinkWriterFromURL oMFCreateSinkWriterFromURL;
// tIMFSinkWriter_SetInputMediaType oIMFSinkWriter_SetInputMediaType;
// tIMFSinkWriter_AddStream oIMFSinkWriter_AddStream;
// tIMFSinkWriter_WriteSample oIMFSinkWriter_WriteSample;
// tIMFSinkWriter_Finalize oIMFSinkWriter_Finalize;
// tOMSetRenderTargets oOMSetRenderTargets;
// tGetRenderTimeBase oGetRenderTimeBase;
// tGetGameSpeedMultiplier oGetGameSpeedMultiplier;
// tCreateThread oCreateThread;
// tWaitForSingleObject oWaitForSingleObject;
// tStepAudio oStepAudio;
// tCreateTexture oCreateTexture;
// tCreateSourceVoice oCreateSourceVoice;
// tSubmitSourceBuffer oSubmitSourceBuffer;
// tDraw oDraw;
// tAudioUnk01 oAudioUnk01;

// void avlog_callback(void* ptr, int level, const char* fmt, const va_list vargs) {
//    static char msg[8192];
//    vsnprintf_s(msg, sizeof(msg), fmt, vargs);
//    Logger::instance().write(Logger::instance().getTimestamp(), " ", Logger::instance().getLogLevelString(LL_NON), "
//    ",
//                             Logger::instance().getThreadId(), " AVCODEC: ");
//    if (ptr) {
//        const AVClass* avc = *static_cast<AVClass**>(ptr);
//        Logger::instance().write("(");
//        Logger::instance().write(avc->item_name(ptr));
//        Logger::instance().write("): ");
//    }
//    Logger::instance().write(msg);
//}

// static HRESULT CreateSourceVoice(
//	IXAudio2                    *pThis,
//	IXAudio2SourceVoice         **ppSourceVoice,
//	const WAVEFORMATEX          *pSourceFormat,
//	UINT32                      Flags,
//	float                       MaxFrequencyRatio,
//	IXAudio2VoiceCallback       *pCallback,
//	const XAUDIO2_VOICE_SENDS   *pSendList,
//	const XAUDIO2_EFFECT_CHAIN  *pEffectChain
//	) {
//	PRE();
//	HRESULT result = oCreateSourceVoice(pThis, ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback,
// pSendList, pEffectChain); 	if (SUCCEEDED(result)) { 		hookVirtualFunction(*ppSourceVoice, 21,
// &SubmitSourceBuffer, &oSubmitSourceBuffer, hkSubmitSourceBuffer);
//	}
//	//WAVEFORMATEX *pSourceFormatRW = (WAVEFORMATEX *)pSourceFormat;
//	//pSourceFormatRW->nSamplesPerSec = pSourceFormat->nSamplesPerSec;
//	POST();
//	return result;
//}

// static HRESULT SubmitSourceBuffer(
//	IXAudio2SourceVoice      *pThis,
//	const XAUDIO2_BUFFER     *pBuffer,
//	const XAUDIO2_BUFFER_WMA *pBufferWMA
//	) {
//	//StackDump(128, __func__);
//	return oSubmitSourceBuffer(pThis, pBuffer, pBufferWMA);
//	//return S_OK;
//}

// void onKeyboardMessage(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended, BOOL isWithAlt, BOOL wasDownBefore,
//                        BOOL isUpNow) {
//     if (key == VK_MULTIPLY) {
//         // VKENCODERCONFIG vkConfig, vkConfig2;
//
//         VKENCODERCONFIG vkConfig{
//             .version = 1,
//             .video{.encoder{"libx264"},
//                    .options{"_pixelFormat=yuv420p|crf=17.000|opencl=1|preset=medium|rc=crf|"
//                             "x264-params=qpmax=22:aq-mode=2:aq-strength=0.700:rc-lookahead=180:"
//                             "keyint=480:min-keyint=3:bframes=11:b-adapt=2:ref=3:deblock=0:0:direct="
//                             "auto:me=umh:merange=32:subme=10:trellis=2:no-fast-pskip=1"},
//                    .filters{""},
//                    .sidedata{""}},
//             .audio{.encoder{"aac"},                                          // @clang-format
//                    .options{"_sampleFormat=fltp|b=320000|profile=aac_main"}, // @clang-format
//                    .filters{""},                                             // @clang-format
//                    .sidedata{""}},
//             .format = {.container{"mp4"}, .faststart = true}};
//
//         BOOL isOK = false;
//         ShowCursor(TRUE);
//         IVoukoder* pVoukoder = nullptr;
//         // ACTCTX* actctx = nullptr;
//
//         // ASSERT_RUNTIME(GetCurrentActCtx(reinterpret_cast<PHANDLE>(&actctx)),
//         //"Failed to get Activation Context for current thread.");
//         // REQUIRE(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Failed to initialize COM.");
//         REQUIRE(CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder,
//                                  reinterpret_cast<void**>(&pVoukoder)),
//                 "Failed to create an instance of IVoukoder interface.");
//         REQUIRE(pVoukoder->SetConfig(vkConfig), "Failed to set Voukoder config.");
//
//         REQUIRE(pVoukoder->ShowVoukoderDialog(true, true, &isOK, nullptr, GetModuleHandle(nullptr)),
//                 "Failed to show Voukoder dialog.");
//
//         if (isOK) {
//             LOG(LL_NFO, "Updating configuration...");
//             REQUIRE(pVoukoder->GetConfig(&vkConfig), "Failed to get config from Voukoder.");
//             config::encoder_config = vkConfig;
//             LOG_CALL(LL_DBG, config::writeEncoderConfig());
//
//         } else {
//             LOG(LL_NFO, "Voukoder config cancelled by user.");
//         }
//     }
// }

void onPresent(IDXGISwapChain* swapChain) {
    mainSwapChain = swapChain;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        try {
            ComPtr<ID3D11Device> pDevice;
            ComPtr<ID3D11DeviceContext> pDeviceContext;
            ComPtr<ID3D11Texture2D> texture;
            DXGI_SWAP_CHAIN_DESC desc;

            swapChain->GetDesc(&desc);

            LOG(LL_NFO, "BUFFER COUNT: ", desc.BufferCount);
            REQUIRE(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)texture.GetAddressOf()),
                    "Failed to get the texture buffer");
            REQUIRE(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)pDevice.GetAddressOf()),
                    "Failed to get the D3D11 device");
            pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
            NOT_NULL(pDeviceContext.Get(), "Failed to get D3D11 device context");

            // REQUIRE(ID3D11DeviceContextHooks3::Draw::PerformHook(pDeviceContext.Get()), "Failed to hook
            // ID3D11DeviceContextHooks::Draw");
            PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, Draw, pDeviceContext.Get());
            PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, OMSetRenderTargets, pDeviceContext.Get());
            /*REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 13, &Draw, &oDraw, hkDraw),
                    "Failed to hook ID3DDeviceContext::Draw");*/
            /*REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 33, &Hook_OMSetRenderTargets, &oOMSetRenderTargets,
                                        hkOMSetRenderTargets),
                    "Failed to hook ID3DDeviceContext::OMSetRenderTargets");*/
            ComPtr<IDXGIDevice> pDXGIDevice;
            REQUIRE(pDevice.As(&pDXGIDevice), "Failed to get IDXGIDevice from ID3D11Device");

            ComPtr<IDXGIAdapter> pDXGIAdapter;
            REQUIRE(pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)pDXGIAdapter.GetAddressOf()),
                    "Failed to get IDXGIAdapter");
            REQUIRE(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)pDXGIFactory.GetAddressOf()),
                    "Failed to get IDXGIFactory");

            prepareDeferredContext(pDevice, pDeviceContext);

        } catch (std::exception& ex) {
            LOG(LL_ERR, ex.what());
        }
    }

    // Process Async Queue
    {
        std::lock_guard<std::mutex> asyncQueueLock(mxAsyncQueue);
        while (!asyncQueue.empty()) {
            if (auto& task = asyncQueue.front(); task.valid()) {
                task();
            }
            asyncQueue.pop();
        }
    }
}

static HRESULT IDXGISwapChainHooks::Present::Implementation(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags) {
    if (!mainSwapChain) {
        onPresent(pThis);
    }
    return IDXGISwapChainHooks::Present::OriginalFunc(pThis, SyncInterval, Flags);
}

void initialize() {
    PRE();

    try {
        mainThreadId = std::this_thread::get_id();

        /*REQUIRE(CoInitializeEx(NULL, COINIT_MULTITHREADED), "Failed to initialize COM");

        ComPtr<IXAudio2> pXAudio;
        REQUIRE(XAudio2Create(pXAudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR), "Failed to create XAudio2
        object");*/
        // REQUIRE(hookVirtualFunction(pXAudio.Get(), 11, &CreateSourceVoice, &oCreateSourceVoice,
        // hkCreateSourceVoice), "Failed to hook IXAudio2::CreateSourceVoice"); IXAudio2MasteringVoice
        // *pMasterVoice; WAVEFORMATEX waveFormat;

        // UINT32 deviceCount; // Number of devices
        // pXAudio->GetDeviceCount(&deviceCount);

        // XAUDIO2_DEVICE_DETAILS deviceDetails;	// Structure to hold details about audio device
        // int preferredDevice = 0;				// Device to be used

        //										// Loop through devices
        // for (unsigned int i = 0; i < deviceCount; i++)
        //{
        //	// Get device details
        //	pXAudio->GetDeviceDetails(i, &deviceDetails);
        //	// Check conditions ( default device )
        //	if (deviceDetails.Role == DefaultCommunicationsDevice)
        //	{
        //		preferredDevice = i;
        //		break;
        //	}

        //}

        /*REQUIRE(hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL,
                                  &oMFCreateSinkWriterFromURL, hkMFCreateSinkWriterFromURL),
                "Failed to hook MFCreateSinkWriterFromURL in mfreadwrite.dll");*/

        PERFORM_NAMED_IMPORT_HOOK_REQUIRED("mfreadwrite.dll", MFCreateSinkWriterFromURL);
        // REQUIRE(hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance,
        // hkCoCreateInstance), "Failed to hook CoCreateInstance in ole32.dll");

        pYaraHelper.reset(new YaraHelper());
        pYaraHelper->initialize();

        MODULEINFO info;
        GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &info, sizeof(info));
        LOG(LL_NFO, "Image base:", ((void*)info.lpBaseOfDll));

        uint64_t pGetRenderTimeBase = NULL;
        uint64_t pCreateTexture = NULL;
        // uint64_t pLinearizeTexture = NULL;
        // uint64_t pAudioUnk01 = NULL;
        // uint64_t pGetGameSpeedMultiplier = NULL;
        // uint64_t pStepAudio = NULL;
        uint64_t pCreateThread = NULL;
        // uint64_t pWaitForSingleObject = NULL;
        pYaraHelper->addEntry("yara_get_render_time_base_function", yara_get_render_time_base_function,
                              &pGetRenderTimeBase);
        // pYaraHelper->addEntry("yara_get_game_speed_multiplier_function", yara_get_game_speed_multiplier_function,
        //                      &pGetGameSpeedMultiplier);
        // pYaraHelper->addEntry("yara_step_audio_function", yara_step_audio_function, &pStepAudio);
        // pYaraHelper->addEntry("yara_audio_unk01_function", yara_audio_unk01_function, &pAudioUnk01);
        pYaraHelper->addEntry("yara_create_thread_function", yara_create_thread_function, &pCreateThread);
        // pYaraHelper->addEntry("yara_create_export_texture_function", yara_create_export_texture_function,
        // &pCreateExportTexture);
        pYaraHelper->addEntry("yara_create_texture_function", yara_create_texture_function, &pCreateTexture);
        // pYaraHelper->addEntry("yara_wait_for_single_object", yara_wait_for_single_object, &pWaitForSingleObject);
        /*pYaraHelper->addEntry("yara_global_unk01_command", yara_global_unk01_command, &pGlobalUnk01Cmd);
        pYaraHelper->addEntry("yara_get_var_ptr_by_hash_2", yara_get_var_ptr_by_hash_2, &pGetVarPtrByHash);*/
        pYaraHelper->performScan();
        // LOG(LL_NFO, (VOID*)(0x311C84 + (intptr_t)info.lpBaseOfDll));

        // REQUIRE(hookX64Function((BYTE*)(0x11441F4 + (intptr_t)info.lpBaseOfDll), &Detour_GetVarHash, &oGetVarHash,
        // hkGetVar), "Failed to hook GetVar function.");
        try {
            if (pGetRenderTimeBase) {
                PERFORM_X64_HOOK_REQUIRED(GetRenderTimeBase, pGetRenderTimeBase);
                isCustomFrameRateSupported = true;
            } else {
                LOG(LL_ERR, "Could not find the address for FPS function.");
                LOG(LL_ERR, "Custom FPS support is DISABLED!!!");
            }

            if (pCreateTexture) {
                PERFORM_X64_HOOK_REQUIRED(CreateTexture, pCreateTexture);
                /*REQUIRE(hookX64Function(pCreateTexture, &Detour_CreateTexture, &oCreateTexture, hkCreateTexture),
                        "Failed to hook CreateTexture function.");*/
            } else {
                LOG(LL_ERR, "Could not find the address for CreateTexture function.");
            }

            if (pCreateThread) {
                PERFORM_X64_HOOK_REQUIRED(CreateThread, pCreateThread);
                /*REQUIRE(hookX64Function(pCreateThread, &Detour_CreateThread, &oCreateThread, hkCreateThread),
                        "Failed to hook CreateThread function.");*/
            } else {
                LOG(LL_ERR, "Could not find the address for CreateThread function.");
            }

            /*if (pWaitForSingleObject) {
                    REQUIRE(hookX64Function(pWaitForSingleObject, &Detour_WaitForSingleObject, &oWaitForSingleObject,
            hkWaitForSingleObject), "Failed to hook WaitForSingleObject function."); } else { LOG(LL_ERR, "Could not
            find the address for WaitForSingleObject function.");
            }*/

            /*if (pStepAudio) {
                    REQUIRE(hookX64Function(pStepAudio, &Detour_StepAudio, &oStepAudio, hkStepAudio), "Failed to hook
            StepAudio function."); } else { LOG(LL_ERR, "Could not find the address for StepAudio.");
            }
*/
            /*if (pGetGameSpeedMultiplier) {
                    REQUIRE(hookX64Function(pGetGameSpeedMultiplier, &Detour_GetGameSpeedMultiplier,
            &oGetGameSpeedMultiplier, hkGetGameSpeedMultiplier), "Failed to hook GetGameSpeedMultiplier function."); }
            else { LOG(LL_ERR, "Could not find the address for GetGameSpeedMultiplier function.");
            }*/

            /*if (pAudioUnk01) {
                    REQUIRE(hookX64Function(pAudioUnk01, &Detour_AudioUnk01, &oAudioUnk01, hkAudioUnk01), "Failed to
            hook AudioUnk01 function."); } else { LOG(LL_ERR, "Could not find the address for AudioUnk01 function.");
            }*/

            /*if (pCreateExportTexture) {
                    REQUIRE(hookX64Function(pCreateExportTexture, &Detour_CreateTexture, &oCreateTexture,
            hkCreateExportTexture), "Failed to hook CreateExportTexture function.");
            }*/

            /*if (pGlobalUnk01Cmd) {
                    uint32_t offset = *(uint32_t*)((uint8_t*)pGlobalUnk01Cmd + 3);
                    pGlobalUnk01 = (uint8_t*)pGlobalUnk01Cmd + offset + 7;
                    LOG(LL_DBG, "pGlobalUnk01: ", pGlobalUnk01);
            } else {
                    LOG(LL_ERR, "Could not find the address for global var: pGlobalUnk0");
            }

            if (pGetVarPtrByHash) {
                    REQUIRE(hookX64Function(pGetVarPtrByHash, &Detour_GetVarPtrByHash2, &oGetVarPtrByHash2,
                    HHHHHHHHHHHHHHHHY
            }*/

        } catch (std::exception& ex) {
            LOG(LL_ERR, ex.what());
        }

        ////0x14CC6AC;
        // REQUIRE(hookX64Function((BYTE*)(0x14CC6AC + (intptr_t)info.lpBaseOfDll), &Detour_GetFrameRate,
        // &oGetFrameRate, hkGetFrameRate), "Failed to hook frame rate function.");
        ////0x14CC64C;
        // REQUIRE(hookX64Function((BYTE*)(0x14CC64C + (intptr_t)info.lpBaseOfDll), &Detour_GetAudioSamples,
        // &oGetAudioSamples, hkGetAudioSamples), "Failed to hook audio sample function."); 0x14CBED8;
        // REQUIRE(hookX64Function((BYTE*)(0x14CBED8 + (intptr_t)info.lpBaseOfDll), &Detour_getFrameRateFraction,
        // &ogetFrameRateFraction, hkGetFrameRateFraction), "Failed to hook audio sample function."); 0x87CC80;
        // REQUIRE(hookX64Function((BYTE*)(0x87CC80 + (intptr_t)info.lpBaseOfDll), &Detour_Unk01, &oUnk01, hkUnk01),
        // "Failed to hook audio sample function."); 0x4BB744; 0x754884 REQUIRE(hookX64Function((BYTE*)(0x754884 +
        // (intptr_t)info.lpBaseOfDll), &Detour_GetTexture, &oGetTexture, hkGetTexture), "Failed to hook GetTexture
        // function."); 0x11C4980 REQUIRE(hookX64Function((BYTE*)(0x11C4980 + (intptr_t)info.lpBaseOfDll),
        // &Detour_GetGlobalVariableIndex, &oGetGlobalVariableIndex, hkGetGlobalVariableIndex), "Failed to hook
        // GetSomething function."); 0x1552198 REQUIRE(hookX64Function((BYTE*)(0x1552198 + (intptr_t)info.lpBaseOfDll),
        // &Detour_GetVariable, &oGetVariable, hkGetVariable), "Failed to hook GetVariable function.");
        // REQUIRE(hookX64Function((BYTE*)(0x15546F8 + (intptr_t)info.lpBaseOfDll), &Detour_GetVariable, &oGetVariable,
        // hkGetVariable), "Failed to hook GetVariable function."); 11CA828 REQUIRE(hookX64Function((BYTE*)(0x11CA828 +
        // (intptr_t)info.lpBaseOfDll), &Detour_GetMatrices, &oGetMatrices, hkGetMatrices), "Failed to hook GetMatrices
        // function."); 11441F4 0x352A3C REQUIRE(hookX64Function((BYTE*)(0x352A3C + (intptr_t)info.lpBaseOfDll),
        // &Detour_GetVarPtrByHash, &oGetVarPtrByHash, hkGetVarPtrByHash), "Failed to hook GetVarPtrByHash function.");

        // LOG_CALL(LL_DBG, av_register_all());
        // LOG_CALL(LL_DBG, avcodec_register_all());
        // LOG_CALL(LL_DBG, av_log_set_level(AV_LOG_TRACE));
        // LOG_CALL(LL_DBG, av_log_set_callback(&avlog_callback));
    } catch (std::exception& ex) {
        // TODO cleanup
        POST();
        throw ex;
    }
    POST();
}

void ScriptMain() {
    PRE();
    LOG(LL_NFO, "Starting main loop");
    while (true) {
        WAIT(0);
    }
    POST();
}

// static HRESULT Hook_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid,
//                                     LPVOID *ppv) {
//    PRE();
//    HRESULT result = oCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
//
//    /*if (IsEqualGUID(riid, _uuidof(IXAudio2))) {
//            IXAudio2* pXAudio = (IXAudio2*)(*ppv);
//            REQUIRE(hookVirtualFunction(pXAudio, 8, &CreateSourceVoice, &oCreateSourceVoice, hkCreateSourceVoice),
//    "Failed to hook IXAudio2::CreateSourceVoice");
//    }*/
//
//    char buffer[64];
//    GUIDToString(rclsid, buffer, 64);
//    LOG(LL_NFO, "CoCreateInstance: ", buffer);
//    POST();
//    return result;
//}

static void
ID3D11DeviceContextHooks::OMSetRenderTargets::Implementation(ID3D11DeviceContext* pThis, UINT NumViews,
                                                             ID3D11RenderTargetView* const* ppRenderTargetViews,
                                                             ID3D11DepthStencilView* pDepthStencilView) {
    PRE();
    if (::exportContext) {
        // LOG(LL_TRC, "Trying to find out if this ID3D11DeviceContext is linear depth buffer...");
        for (uint32_t i = 0; i < NumViews; i++) {
            if (ppRenderTargetViews[i]) {
                ComPtr<ID3D11Resource> pResource;
                ComPtr<ID3D11Texture2D> pTexture2D;
                ppRenderTargetViews[i]->GetResource(pResource.GetAddressOf());
                if (SUCCEEDED(pResource.As(&pTexture2D))) {
                    if (pTexture2D.Get() == pGameDepthBufferQuarterLinear.Get()) {
                        LOG(LL_DBG, " i:", i, " num:", NumViews, " dsv:", (void*)pDepthStencilView);
                        pCtxLinearizeBuffer = pThis;
                    }
                }
            }
        }
    }

    ComPtr<ID3D11Resource> pRTVTexture;
    if ((ppRenderTargetViews) && (ppRenderTargetViews[0])) {
        LOG_CALL(LL_TRC, ppRenderTargetViews[0]->GetResource(pRTVTexture.GetAddressOf()));
    }

    ComPtr<ID3D11RenderTargetView> pOldRTV;
    ComPtr<ID3D11Resource> pOldRTVTexture;
    pThis->OMGetRenderTargets(1, pOldRTV.GetAddressOf(), NULL);
    if (pOldRTV) {
        LOG_CALL(LL_TRC, pOldRTV->GetResource(pOldRTVTexture.GetAddressOf()));
    }

    if (::exportContext != nullptr && ::exportContext->pExportRenderTarget != nullptr &&
        (::exportContext->pExportRenderTarget == pRTVTexture)) {

        // Time to capture rendered frame
        try {
            ComPtr<ID3D11Device> pDevice;
            pThis->GetDevice(pDevice.GetAddressOf());

            ComPtr<ID3D11Texture2D> pDepthBufferCopy = nullptr;
            ComPtr<ID3D11Texture2D> pBackBufferCopy = nullptr;
            ComPtr<ID3D11Texture2D> pStencilBufferCopy = nullptr;

            if (config::export_openexr) {
                {
                    D3D11_TEXTURE2D_DESC desc;
                    pLinearDepthTexture->GetDesc(&desc);

                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
                    desc.BindFlags = 0;
                    desc.MiscFlags = 0;
                    desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

                    REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pDepthBufferCopy.GetAddressOf()),
                            "Failed to create depth buffer copy texture");

                    pThis->CopyResource(pDepthBufferCopy.Get(), pLinearDepthTexture.Get());
                }
                {
                    D3D11_TEXTURE2D_DESC desc;
                    pGameBackBufferResolved->GetDesc(&desc);
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
                    desc.BindFlags = 0;
                    desc.MiscFlags = 0;
                    desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

                    REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pBackBufferCopy.GetAddressOf()),
                            "Failed to create back buffer copy texture");

                    pThis->CopyResource(pBackBufferCopy.Get(), pGameBackBufferResolved.Get());
                }
                {
                    D3D11_TEXTURE2D_DESC desc;
                    pStencilTexture->GetDesc(&desc);
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    desc.BindFlags = 0;
                    desc.MiscFlags = 0;
                    desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

                    REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pStencilBufferCopy.GetAddressOf()),
                            "Failed to create stencil buffer copy");

                    // pThis->ResolveSubresource(pStencilBufferCopy.Get(), 0, pGameDepthBuffer.Get(), 0,
                    // DXGI_FORMAT::DXGI_FORMAT_R32G8X24_TYPELESS);
                    pThis->CopyResource(pStencilBufferCopy.Get(), pGameEdgeCopy.Get());
                }
                {
                    std::lock_guard<std::mutex> sessionLock(mxSession);
                    if ((encodingSession != nullptr) && (encodingSession->isCapturing)) {
                        encodingSession->enqueueEXRImage(pThis, pBackBufferCopy, pDepthBufferCopy, pStencilBufferCopy);
                    }
                }
            }

            ComPtr<ID3D11Texture2D> pSwapChainBuffer;
            REQUIRE(::exportContext->pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                                           (void**)pSwapChainBuffer.GetAddressOf()),
                    "Failed to get swap chain's buffer");

            // IDXGISwapChainHooks::Present::OriginalFunc(::exportContext->pSwapChain.Get(), 0, 0);
            //float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            //::exportContext->pDeviceContext->ClearRenderTargetView((ID3D11RenderTargetView*)pSwapChainBuffer.Get(),
            //                                                       color);
            LOG_CALL(LL_DBG, ::exportContext->pSwapChain->Present(1, 0)); // IMPORTANT: This call makes ENB and ReShade
                                                                          // effects to be applied to the render target
            // LOG_CALL(LL_DBG, ::exportContext->pSwapChain->Present(
            //                      0, 0)); // IMPORTANT: This call makes ENB and ReShade effects to be
            //  applied to the render target
            // LOG_CALL(LL_DBG, ::exportContext->pSwapChain->Present(
            //                      0, 0)); // IMPORTANT: This call makes ENB and ReShade effects to be
            //                                              // applied to the render target

            // BEGIN CPU
            // auto& image_ref = *(::exportContext->capturedImage);
            // LOG_CALL(LL_DBG,
            //         DirectX::CaptureTexture(::exportContext->pDevice.Get(), ::exportContext->pDeviceContext.Get(),
            //                                 pSwapChainBuffer.Get(), image_ref));
            // if (::exportContext->capturedImage->GetImageCount() == 0) {
            //    LOG(LL_ERR, "There is no image to capture.");
            //    throw std::exception();
            //}
            // const DirectX::Image* image = ::exportContext->capturedImage->GetImage(0, 0, 0);
            // NOT_NULL(image, "Could not get current frame.");
            // NOT_NULL(image->pixels, "Could not get current frame.");
            // END CPU
            float current_shutter_position = FLT_MAX;
            if (config::motion_blur_samples != 0) {
                current_shutter_position = (::exportContext->totalFrameNum % (config::motion_blur_samples + 1)) /
                                           (float)config::motion_blur_samples;
                if (current_shutter_position >= (1 - config::motion_blur_strength)) {
                    drawAdditive(pDevice, pThis, pSwapChainBuffer);
                }
                // D3D11_MAPPED_SUBRESOURCE mapped;

                // LOG_CALL(LL_DBG, pDContext->CopyResource(pSwapBufferCopy.Get(), pSwapChainBuffer.Get()));
                // REQUIRE(pThis->Map(pSwapBufferCopy.Get(), 0, D3D11_MAP_READ, 0, &mapped),
                //         "Failed to capture swapbuffer.");

                //{
                //    std::lock_guard<std::mutex> sessionLock(mxSession);
                //    if ((encodingSession != nullptr) && (encodingSession->isCapturing)) {
                //        // CPU
                //        // REQUIRE(encodingSession->enqueueVideoFrame(image->pixels, image->rowPitch,
                //        // image->slicePitch),
                //        //        "Failed to enqueue frame.");
                //        REQUIRE(encodingSession->enqueueVideoFrame(mapped), "Failed to enqueue frame.");
                //    }
                //}

                // pThis->Unmap(pSwapBufferCopy.Get(), 0);
            } else {
                // Trick to use the same buffers for when not using motion blur
                ::exportContext->accCount = 0;
                drawAdditive(pDevice, pThis, pSwapChainBuffer);
                ::exportContext->accCount = 1;
                ::exportContext->totalFrameNum = 1;
            }

            if ((::exportContext->totalFrameNum % (::config::motion_blur_samples + 1)) == config::motion_blur_samples) {
                ComPtr<ID3D11Texture2D> result;

                result = divideBuffer(pDevice, pThis, ::exportContext->accCount);
                ::exportContext->accCount = 0;
                // TODO: Fix this memory leak made for testing

                D3D11_MAPPED_SUBRESOURCE mapped;

                pThis->Map(result.Get(), 0, D3D11_MAP_READ, 0, &mapped);

                {
                    std::lock_guard<std::mutex> sessionLock(mxSession);
                    if ((encodingSession != nullptr) && (encodingSession->isCapturing)) {
                        // CPU
                        // REQUIRE(encodingSession->enqueueVideoFrame(image->pixels, image->rowPitch,
                        // image->slicePitch),
                        //        "Failed to enqueue frame.");
                        REQUIRE(encodingSession->enqueueVideoFrame(mapped), "Failed to enqueue frame.");
                    }
                }
            }
            ::exportContext->totalFrameNum++;

            //::exportContext->capturedImage->Release();
        } catch (std::exception&) {
            LOG(LL_ERR, "Reading video frame from D3D Device failed.");
            //::exportContext->capturedImage->Release();
            LOG_CALL(LL_DBG, encodingSession.reset());
            LOG_CALL(LL_DBG, ::exportContext.reset());
        }
    }
    LOG_CALL(LL_TRC, ID3D11DeviceContextHooks::OMSetRenderTargets::OriginalFunc(pThis, NumViews, ppRenderTargetViews,
                                                                                pDepthStencilView));
    POST();
}

static HRESULT ImportHooks::MFCreateSinkWriterFromURL::Implementation(LPCWSTR pwszOutputURL, IMFByteStream* pByteStream,
                                                                      IMFAttributes* pAttributes,
                                                                      IMFSinkWriter** ppSinkWriter) {
    PRE();
    HRESULT result =
        ImportHooks::MFCreateSinkWriterFromURL::OriginalFunc(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
    if (SUCCEEDED(result)) {
        try {
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, AddStream, *ppSinkWriter);
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, SetInputMediaType, *ppSinkWriter);
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, WriteSample, *ppSinkWriter);
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, Finalize, *ppSinkWriter);
            /*REQUIRE(hookVirtualFunction(*ppSinkWriter, 3, &Hook_IMFSinkWriter_AddStream,
            &oIMFSinkWriter_AddStream, hkIMFSinkWriter_AddStream), "Failed to hook IMFSinkWriter::AddStream");
            REQUIRE(hookVirtualFunction(*ppSinkWriter, 4, &IMFSinkWriter_SetInputMediaType,
                                        &oIMFSinkWriter_SetInputMediaType, hkIMFSinkWriter_SetInputMediaType),
                    "Failed to hook IMFSinkWriter::SetInputMediaType");
            REQUIRE(hookVirtualFunction(*ppSinkWriter, 6, &Hook_IMFSinkWriter_WriteSample,
            &oIMFSinkWriter_WriteSample, hkIMFSinkWriter_WriteSample), "Failed to hook IMFSinkWriter::WriteSample");
            REQUIRE(hookVirtualFunction(*ppSinkWriter, 11, &Hook_IMFSinkWriter_Finalize, &oIMFSinkWriter_Finalize,
                                        hkIMFSinkWriter_Finalize),
                    "Failed to hook IMFSinkWriter::Finalize");*/
        } catch (...) {
            LOG(LL_ERR, "Hooking IMFSinkWriter functions failed");
        }
    }
    POST();
    return result;
}

static HRESULT IMFSinkWriterHooks::AddStream::Implementation(IMFSinkWriter* pThis, IMFMediaType* pTargetMediaType,
                                                             DWORD* pdwStreamIndex) {
    PRE();
    LOG(LL_NFO, "IMFSinkWriter::AddStream: ", GetMediaTypeDescription(pTargetMediaType).c_str());
    POST();
    return IMFSinkWriterHooks::AddStream::OriginalFunc(pThis, pTargetMediaType, pdwStreamIndex);
}

static HRESULT IMFSinkWriterHooks::SetInputMediaType::Implementation(IMFSinkWriter* pThis, DWORD dwStreamIndex,
                                                                     IMFMediaType* pInputMediaType,
                                                                     IMFAttributes* pEncodingParameters) {
    PRE();
    LOG(LL_NFO, "IMFSinkWriter::SetInputMediaType: ", GetMediaTypeDescription(pInputMediaType).c_str());

    GUID majorType;
    if (SUCCEEDED(pInputMediaType->GetMajorType(&majorType))) {
        if (IsEqualGUID(majorType, MFMediaType_Video)) {
            ::exportContext->videoMediaType = pInputMediaType;
        } else if (IsEqualGUID(majorType, MFMediaType_Audio)) {
            try {
                std::lock_guard<std::mutex> sessionLock(mxSession);
                UINT width, height, fps_num, fps_den;
                MFGetAttribute2UINT32asUINT64(::exportContext->videoMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
                MFGetAttributeRatio(::exportContext->videoMediaType.Get(), MF_MT_FRAME_RATE, &fps_num, &fps_den);

                GUID pixelFormat;
                ::exportContext->videoMediaType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

                DXGI_SWAP_CHAIN_DESC desc;
                ::exportContext->pSwapChain->GetDesc(&desc);

                if (isCustomFrameRateSupported) {
                    auto fps = config::fps;
                    fps_num = fps.first;
                    fps_den = fps.second;

                    float gameFrameRate =
                        ((float)fps.first * ((float)config::motion_blur_samples + 1) / (float)fps.second);
                    if (gameFrameRate > 60.0f) {
                        LOG(LL_NON, "fps * (motion_blur_samples + 1) > 60.0!!!");
                        LOG(LL_NON, "Audio export will be disabled!!!");

                        //::exportContext->audioSkip = gameFrameRate / 60;
                        //::exportContext->audioSkipCounter = ::exportContext->audioSkip;
                        ::exportContext->isAudioExportDisabled = true;
                    }
                }

                // REQUIRE(session->createVideoContext(desc.BufferDesc.Width, desc.BufferDesc.Height, "bgra",
                // fps_num, fps_den, config::motion_blur_samples,  config::video_fmt, config::video_enc,
                // config::video_cfg), "Failed to create video context");

                UINT32 blockAlignment, numChannels, sampleRate, bitsPerSample;
                GUID subType;

                pInputMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
                pInputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
                pInputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
                pInputMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
                pInputMediaType->GetGUID(MF_MT_SUBTYPE, &subType);

                /*if (IsEqualGUID(subType, MFAudioFormat_PCM)) {
                        REQUIRE(session->createAudioContext(numChannels, sampleRate, bitsPerSample, "s16",
                blockAlignment, config::audio_fmt, config::audio_enc, config::audio_cfg), "Failed to create audio
                context."); } else { char buffer[64]; GUIDToString(subType, buffer, 64); LOG(LL_ERR, "Unsupported
                input audio format: ", buffer); throw std::runtime_error("Unsupported input audio format");
                }*/

                char buffer[128];
                std::string output_file = config::output_dir;
                std::string exrOutputPath;

                output_file += "\\EVE-";
                time_t rawtime;
                struct tm timeinfo;
                time(&rawtime);
                localtime_s(&timeinfo, &rawtime);
                strftime(buffer, 128, "%Y%m%d%H%M%S", &timeinfo);
                output_file += buffer;
                exrOutputPath = std::regex_replace(output_file, std::regex("\\\\+"), "\\");
                output_file += ".mp4";

                std::string filename = std::regex_replace(output_file, std::regex("\\\\+"), "\\");

                LOG(LL_NFO, "Output file: ", filename);

                /*IVoukoder* tmpVoukoder = nullptr;
                REQUIRE(CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder,
                                         reinterpret_cast<void**>(&tmpVoukoder)),*/
                //"Failed to create an instance of IVoukoder interface.");
                // ComPtr<IVoukoder> pVoukoder = tmpVoukoder;
                // tmpVoukoder = nullptr;

                /*REQUIRE(tmpVoukoder->ShowVoukoderDialog(true, true, &isOK, nullptr, nullptr),
                        "Failed to show Voukoder dialog.");*/

                /*ASSERT_RUNTIME(isOK, "Exporting was cancelled.");*/

                // IVoukoder* pVoukoder = nullptr;
                // VKENCODERCONFIG vkConfig{
                //    .version = 1,
                //    .video{.encoder{"libx264"},
                //           .options{"_pixelFormat=yuv420p|crf=17.000|opencl=1|preset=medium|rc=crf|"
                //                    "x264-params=qpmax=22:aq-mode=2:aq-strength=0.700:rc-lookahead=180:"
                //                    "keyint=480:min-keyint=3:bframes=11:b-adapt=2:ref=3:deblock=0:0:direct="
                //                    "auto:me=umh:merange=32:subme=10:trellis=2:no-fast-pskip=1"},
                //           .filters{""},
                //           .sidedata{""}},
                //    .audio{.encoder{"aac"},                                          // @clang-format
                //           .options{"_sampleFormat=fltp|b=320000|profile=aac_main"}, // @clang-format
                //           .filters{""},                                             // @clang-format
                //           .sidedata{""}},
                //    .format = {.container{"mp4"}, .faststart = true}};

                // CreateActCtx()
                // REQUIRE(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Failed to initialize COM.");
                // REQUIRE(CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder,
                //                         reinterpret_cast<void**>(&pVoukoder)),
                //"Failed to create an instance of IVoukoder interface.");
                // ComPtr<IVoukoder> pVoukoder = tmpVoukoder;
                // tmpVoukoder = nullptr;

                // VKENCODERCONFIG vkConfig{
                //     .version = 1,
                //     .video{.encoder{"libx264"},
                //            .options{"_pixelFormat=yuv420p|crf=17.000|opencl=1|preset=medium|rc=crf|"
                //                     "x264-params=qpmax=22:aq-mode=2:aq-strength=0.700:rc-lookahead=180:"
                //                     "keyint=480:min-keyint=3:bframes=11:b-adapt=2:ref=3:deblock=0:0:direct="
                //                     "auto:me=umh:merange=32:subme=10:trellis=2:no-fast-pskip=1"},
                //            .filters{""},
                //            .sidedata{""}},
                //     .audio{.encoder{"aac"},                                          // @clang-format
                //            .options{"_sampleFormat=fltp|b=320000|profile=aac_main"}, // @clang-format
                //            .filters{""},                                             // @clang-format
                //            .sidedata{""}},
                //     .format = {.container{"mp4"}, .faststart = true}};
                //  REQUIRE(tmpVoukoder->GetConfig(&vkConfig),
                //  "Failed to get config from
                //  Voukoder."); tmpVoukoder->Release();

                REQUIRE(encodingSession->createContext(config::encoder_config,
                                                       std::wstring(filename.begin(), filename.end()),
                                                       desc.BufferDesc.Width, desc.BufferDesc.Height, "rgba", fps_num,
                                                       fps_den, numChannels, sampleRate, "s16", blockAlignment),
                        "Failed to create encoding context.");

                /*REQUIRE(encodingSession->createContext(
                            config::container_format, filename.c_str(), exrOutputPath, config::format_cfg,
                            desc.BufferDesc.Width, desc.BufferDesc.Height, "bgra", fps_num, fps_den,
                            config::motion_blur_samples, 1 - config::motion_blur_strength, config::video_fmt,
                            config::video_enc, config::video_cfg, numChannels, sampleRate, bitsPerSample, "s16",
                            blockAlignment, config::audio_fmt, config::audio_enc, config::audio_cfg),
                        "Failed to create encoding context.");*/
            } catch (std::exception& ex) {
                LOG(LL_ERR, ex.what());
                LOG_CALL(LL_DBG, encodingSession.reset());
                LOG_CALL(LL_DBG, ::exportContext.reset());
                return MF_E_INVALIDMEDIATYPE;
            }
        }
    }

    POST();
    return IMFSinkWriterHooks::SetInputMediaType::OriginalFunc(pThis, dwStreamIndex, pInputMediaType,
                                                               pEncodingParameters);
}

static HRESULT IMFSinkWriterHooks::WriteSample::Implementation(IMFSinkWriter* pThis, DWORD dwStreamIndex,
                                                               IMFSample* pSample) {
    std::lock_guard sessionLock(mxSession);

    if ((encodingSession) && (dwStreamIndex == 1) && (!::exportContext->isAudioExportDisabled)) {

        ComPtr<IMFMediaBuffer> pBuffer = nullptr;
        try {
            LONGLONG sampleTime;
            pSample->GetSampleTime(&sampleTime);
            pSample->ConvertToContiguousBuffer(pBuffer.GetAddressOf());

            DWORD length;
            pBuffer->GetCurrentLength(&length);
            BYTE* buffer;
            if (SUCCEEDED(pBuffer->Lock(&buffer, NULL, NULL))) {
                try {
                    LOG_CALL(LL_DBG, encodingSession->writeAudioFrame(
                                         buffer, length / 4 /* 2 channels * 2 bytes per sample*/, sampleTime));
                } catch (std::exception& ex) {
                    LOG(LL_ERR, ex.what());
                }
                pBuffer->Unlock();
            }
        } catch (std::exception& ex) {
            LOG(LL_ERR, ex.what());
            LOG_CALL(LL_DBG, encodingSession.reset());
            LOG_CALL(LL_DBG, ::exportContext.reset());
            /*if (pBuffer) {
                pBuffer->Unlock();
            }*/
        }
    }
    /*if (!session) {
            return E_FAIL;
    }*/
    return S_OK;
}

static HRESULT IMFSinkWriterHooks::Finalize::Implementation(IMFSinkWriter* pThis) {
    PRE();
    std::lock_guard<std::mutex> sessionLock(mxSession);
    try {
        if (encodingSession != NULL) {
            LOG_CALL(LL_DBG, encodingSession->finishAudio());
            LOG_CALL(LL_DBG, encodingSession->finishVideo());
            LOG_CALL(LL_DBG, encodingSession->endSession());
        }
    } catch (std::exception& ex) {
        LOG(LL_ERR, ex.what());
    }

    LOG_CALL(LL_DBG, encodingSession.reset());
    LOG_CALL(LL_DBG, ::exportContext.reset());
    POST();
    return S_OK;
}

void finalize() {
    PRE();
    POST();
}

static void ID3D11DeviceContextHooks::Draw::Implementation(ID3D11DeviceContext* pThis, //
                                                           UINT VertexCount,           //
                                                           UINT StartVertexLocation) {
    OriginalFunc(pThis, VertexCount, StartVertexLocation);
    if (pCtxLinearizeBuffer == pThis) {
        pCtxLinearizeBuffer = nullptr;

        ComPtr<ID3D11Device> pDevice;
        pThis->GetDevice(pDevice.GetAddressOf());

        ComPtr<ID3D11RenderTargetView> pCurrentRTV;
        LOG_CALL(LL_DBG, pThis->OMGetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        pCurrentRTV->GetDesc(&rtvDesc);
        LOG_CALL(LL_DBG, pDevice->CreateRenderTargetView(pLinearDepthTexture.Get(), &rtvDesc,
                                                         pCurrentRTV.ReleaseAndGetAddressOf()));
        LOG_CALL(LL_DBG, pThis->OMSetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));

        D3D11_TEXTURE2D_DESC ldtDesc;
        pLinearDepthTexture->GetDesc(&ldtDesc);

        D3D11_VIEWPORT viewport;
        viewport.Width = static_cast<float>(ldtDesc.Width);
        viewport.Height = static_cast<float>(ldtDesc.Height);
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;
        pThis->RSSetViewports(1, &viewport);
        D3D11_RECT rect;
        rect.left = rect.top = 0;
        rect.right = static_cast<LONG>(ldtDesc.Width);
        rect.bottom = static_cast<LONG>(ldtDesc.Height);
        pThis->RSSetScissorRects(1, &rect);

        LOG_CALL(LL_TRC, OriginalFunc(pThis, VertexCount, StartVertexLocation));
    }
}

// static void Detour_StepAudio(void* rcx) {
//	if (::exportContext) {
//		::exportContext->audioSkipCounter += 1.0f;
//		if (::exportContext->audioSkipCounter >= ::exportContext->audioSkip) {
//			oStepAudio(rcx);
//		}
//		while (::exportContext->audioSkipCounter >= ::exportContext->audioSkip) {
//			::exportContext->audioSkipCounter -= ::exportContext->audioSkip;
//		}
//	} else {
//		oStepAudio(rcx);
//	}
//	static int x = 0;
//	if (x == 0) {
//	}
//	x = ++x % 2;
//}

static HANDLE GameHooks::CreateThread::Implementation(void* pFunc, void* pParams, int32_t r8d, int32_t r9d, void* rsp20,
                                                      int32_t rsp28, char* name) {
    PRE();
    void* result = GameHooks::CreateThread::OriginalFunc(pFunc, pParams, r8d, r9d, rsp20, rsp28, name);
    LOG(LL_TRC, "CreateThread:", " pFunc:", pFunc, " pParams:", pParams, " r8d:", Logger::hex(r8d, 4),
        " r9d:", Logger::hex(r9d, 4), " rsp20:", rsp20, " rsp28:", rsp28, " name:", name ? name : "<NULL>");

    // if (name) {
    //	if (std::string("RageAudioMixThread").compare(name) == 0) {
    //		threadIdRageAudioMixThread = ::GetThreadId(result);
    //	}
    //}

    POST();
    return result;
}

// static float Detour_GetGameSpeedMultiplier(void* rcx) {
//	float result = oGetGameSpeedMultiplier(rcx);
//	//if (exportContext) {
//		std::pair<int32_t, int32_t> fps = config::fps;
//		result = 60.0f * (float)fps.second / ((float)fps.first * ((float)config::motion_blur_samples + 1));
//	//}
//	return result;
//}

static float GameHooks::GetRenderTimeBase::Implementation(int64_t choice) {
    PRE();
    const std::pair<int32_t, int32_t> fps = config::fps;
    const float result = 1000.0f * static_cast<float>(fps.second) /
                         (static_cast<float>(fps.first) * (static_cast<float>(config::motion_blur_samples) + 1));
    // float result = 1000.0f / 60.0f;
    LOG(LL_NFO, "Time step: ", result);
    POST();
    return result;
}

// static void Detour_WaitForSingleObject(void* rcx, int32_t edx) {
//	oWaitForSingleObject(rcx, edx);
//	while (threadIdRageAudioMixThread && (threadIdRageAudioMixThread == ::GetCurrentThreadId())) {
//		static int x = 3;
//		x++;
//		x %= 4;
//		ReleaseSemaphore(rcx, 1, NULL);
//		oWaitForSingleObject(rcx, edx);
//		if (x == 0) {
//			break;
//		}
//		Sleep(0);
//	}
//}

// static uint8_t Detour_AudioUnk01(void* rcx) {
//	return 1;
//}

static void* GameHooks::CreateTexture::Implementation(void* rcx, char* name, const uint32_t r8d, const uint32_t width,
                                                      const uint32_t height, const uint32_t format, void* rsp30) {
    void* result = OriginalFunc(rcx, name, r8d, width, height, format, rsp30);

    const auto vresult = static_cast<void**>(result);

    ComPtr<ID3D11Texture2D> pTexture;
    DXGI_FORMAT fmt = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
    if (name && result) {
        if (const auto pUnknown = static_cast<IUnknown*>(*(vresult + 7));
            pUnknown && SUCCEEDED(pUnknown->QueryInterface(pTexture.GetAddressOf()))) {
            D3D11_TEXTURE2D_DESC desc;
            pTexture->GetDesc(&desc);
            fmt = desc.Format;
        }
    }

    LOG(LL_TRC, "CreateTexture:", " rcx:", rcx, " name:", name ? name : "<NULL>", " r8d:", Logger::hex(r8d, 8),
        " width:", width, " height:", height, " fmt:", conv_dxgi_format_to_string(fmt), " result:", result,
        " *r+0:", vresult ? *(vresult + 0) : "<NULL>", " *r+1:", vresult ? *(vresult + 1) : "<NULL>",
        " *r+2:", vresult ? *(vresult + 2) : "<NULL>", " *r+3:", vresult ? *(vresult + 3) : "<NULL>",
        " *r+4:", vresult ? *(vresult + 4) : "<NULL>", " *r+5:", vresult ? *(vresult + 5) : "<NULL>",
        " *r+6:", vresult ? *(vresult + 6) : "<NULL>", " *r+7:", vresult ? *(vresult + 7) : "<NULL>",
        " *r+8:", vresult ? *(vresult + 8) : "<NULL>", " *r+9:", vresult ? *(vresult + 9) : "<NULL>",
        " *r+10:", vresult ? *(vresult + 10) : "<NULL>", " *r+11:", vresult ? *(vresult + 11) : "<NULL>",
        " *r+12:", vresult ? *(vresult + 12) : "<NULL>", " *r+13:", vresult ? *(vresult + 13) : "<NULL>",
        " *r+14:", vresult ? *(vresult + 14) : "<NULL>", " *r+15:", vresult ? *(vresult + 15) : "<NULL>");

    static bool presentHooked = false;
    if (!presentHooked && pTexture && !mainSwapChain) {
        if (auto foregroundWindow = GetForegroundWindow()) {
            ID3D11Device* pTempDevice = nullptr;
            pTexture->GetDevice(&pTempDevice);

            IDXGIDevice* pDXGIDevice = nullptr;
            REQUIRE(pTempDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice),
                    "Failed to get IDXGIDevice");

            IDXGIAdapter* pDXGIAdapter = nullptr;
            REQUIRE(pDXGIDevice->GetAdapter(&pDXGIAdapter), "Failed to get IDXGIAdapter");

            IDXGIFactory* pDXGIFactory = nullptr;
            REQUIRE(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pDXGIFactory),
                    "Failed to get IDXGIFactory");

            DXGI_SWAP_CHAIN_DESC tempDesc{0};
            tempDesc.BufferCount = 1;
            tempDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            tempDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            tempDesc.BufferDesc.Width = 800;
            tempDesc.BufferDesc.Height = 600;
            tempDesc.BufferDesc.RefreshRate = {30, 1};
            tempDesc.OutputWindow = foregroundWindow;
            tempDesc.Windowed = true;
            tempDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            tempDesc.SampleDesc.Count = 1;
            tempDesc.SampleDesc.Quality = 0;

            ComPtr<IDXGISwapChain> pTempSwapChain;
            REQUIRE(pDXGIFactory->CreateSwapChain(pTempDevice, &tempDesc, pTempSwapChain.ReleaseAndGetAddressOf()),
                    "Failed to create temporary swapchain");

            PERFORM_MEMBER_HOOK_REQUIRED(IDXGISwapChain, Present, pTempSwapChain.Get());
            presentHooked = true;
        }
    }

    if (pTexture && name) {
        ComPtr<ID3D11Device> pDevice;
        pTexture->GetDevice(pDevice.GetAddressOf());
        if ((std::strcmp("DepthBuffer_Resolved", name) == 0) || (std::strcmp("DepthBufferCopy", name) == 0)) {
            pGameDepthBufferResolved = pTexture;
        } else if (std::strcmp("DepthBuffer", name) == 0) {
            pGameDepthBuffer = pTexture;
        } else if (std::strcmp("Depth Quarter", name) == 0) {
            pGameDepthBufferQuarter = pTexture;
        } else if (std::strcmp("GBUFFER_0", name) == 0) {
            pGameGBuffer0 = pTexture;
        } else if (std::strcmp("Edge Copy", name) == 0) {
            pGameEdgeCopy = pTexture;
            D3D11_TEXTURE2D_DESC desc;
            pTexture->GetDesc(&desc);
            pDevice->CreateTexture2D(&desc, nullptr, pStencilTexture.GetAddressOf());
        } else if (std::strcmp("Depth Quarter Linear", name) == 0) {
            pGameDepthBufferQuarterLinear = pTexture;
            D3D11_TEXTURE2D_DESC desc, resolvedDesc;

            if (!pGameDepthBufferResolved) {
                pGameDepthBufferResolved = pGameDepthBuffer;
            }

            pGameDepthBufferResolved->GetDesc(&resolvedDesc);
            resolvedDesc.ArraySize = 1;
            resolvedDesc.SampleDesc.Count = 1;
            resolvedDesc.SampleDesc.Quality = 0;
            pGameDepthBufferQuarterLinear->GetDesc(&desc);
            desc.Width = resolvedDesc.Width;
            desc.Height = resolvedDesc.Height;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.CPUAccessFlags = 0;
            LOG_CALL(LL_DBG, pDevice->CreateTexture2D(&desc, NULL, pLinearDepthTexture.GetAddressOf()));
        } else if (std::strcmp("BackBuffer", name) == 0) {
            pGameBackBufferResolved = nullptr;
            pGameDepthBuffer = nullptr;
            pGameDepthBufferQuarter = nullptr;
            pGameDepthBufferQuarterLinear = nullptr;
            pGameDepthBufferResolved = nullptr;
            pGameEdgeCopy = nullptr;
            pGameGBuffer0 = nullptr;

            pGameBackBuffer = pTexture;

            D3D11_TEXTURE2D_DESC desc;

            pGameBackBuffer->GetDesc(&desc);

            // D3D11_TEXTURE2D_DESC swapBufferCopyDesc = desc;
            // swapBufferCopyDesc.MiscFlags = 0;
            // swapBufferCopyDesc.BindFlags = 0;
            // swapBufferCopyDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            // swapBufferCopyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            // swapBufferCopyDesc.Usage = D3D11_USAGE_STAGING;

            // LOG_IF_FAILED(pDevice->CreateTexture2D(&swapBufferCopyDesc, NULL,
            // pSwapBufferCopy.ReleaseAndGetAddressOf()),
            //               "Failed to create swap buffer copy texture");

            D3D11_TEXTURE2D_DESC accBufDesc = desc;
            accBufDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            accBufDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            accBufDesc.CPUAccessFlags = 0;
            accBufDesc.MiscFlags = 0;
            accBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
            accBufDesc.MipLevels = 1;
            accBufDesc.ArraySize = 1;
            accBufDesc.SampleDesc.Count = 1;
            accBufDesc.SampleDesc.Quality = 0;

            LOG_IF_FAILED(pDevice->CreateTexture2D(&accBufDesc, NULL, pMotionBlurAccBuffer.ReleaseAndGetAddressOf()),
                          "Failed to create accumulation buffer texture");

            D3D11_TEXTURE2D_DESC mbBufferDesc = accBufDesc;
            mbBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
            mbBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

            LOG_IF_FAILED(
                pDevice->CreateTexture2D(&mbBufferDesc, NULL, pMotionBlurFinalBuffer.ReleaseAndGetAddressOf()),
                "Failed to create motion blur buffer texture");

            D3D11_RENDER_TARGET_VIEW_DESC accBufRTVDesc;
            accBufRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            accBufRTVDesc.Texture2D.MipSlice = 0;
            accBufRTVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

            LOG_IF_FAILED(pDevice->CreateRenderTargetView(pMotionBlurAccBuffer.Get(), &accBufRTVDesc,
                                                          pRTVAccBuffer.ReleaseAndGetAddressOf()),
                          "Failed to create acc buffer RTV.");

            D3D11_RENDER_TARGET_VIEW_DESC mbBufferRTVDesc;
            mbBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            mbBufferRTVDesc.Texture2D.MipSlice = 0;
            mbBufferRTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

            LOG_IF_FAILED(pDevice->CreateRenderTargetView(pMotionBlurFinalBuffer.Get(), &mbBufferRTVDesc,
                                                          pRTVBlurBuffer.ReleaseAndGetAddressOf()),
                          "Failed to create blur buffer RTV.");

            D3D11_SHADER_RESOURCE_VIEW_DESC accBufSRVDesc;
            accBufSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            accBufSRVDesc.Texture2D.MipLevels = 1;
            accBufSRVDesc.Texture2D.MostDetailedMip = 0;
            accBufSRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

            LOG_IF_FAILED(pDevice->CreateShaderResourceView(pMotionBlurAccBuffer.Get(), &accBufSRVDesc,
                                                            pSRVAccBuffer.ReleaseAndGetAddressOf()),
                          "Failed to create blur buffer SRV.");

            D3D11_TEXTURE2D_DESC sourceSRVTextureDesc = desc;
            sourceSRVTextureDesc.CPUAccessFlags = 0;
            sourceSRVTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            sourceSRVTextureDesc.MiscFlags = 0;
            sourceSRVTextureDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
            sourceSRVTextureDesc.ArraySize = 1;
            sourceSRVTextureDesc.SampleDesc.Count = 1;
            sourceSRVTextureDesc.SampleDesc.Quality = 0;
            sourceSRVTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            sourceSRVTextureDesc.MipLevels = 1;

            REQUIRE(pDevice->CreateTexture2D(&sourceSRVTextureDesc, NULL, pSourceSRVTexture.ReleaseAndGetAddressOf()),
                    "Failed to create back buffer copy texture");

            D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
            sourceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sourceSRVDesc.Texture2D.MipLevels = 1;
            sourceSRVDesc.Texture2D.MostDetailedMip = 0;
            sourceSRVDesc.Format = sourceSRVTextureDesc.Format;

            LOG_IF_FAILED(pDevice->CreateShaderResourceView(pSourceSRVTexture.Get(), &sourceSRVDesc,
                                                            pSourceSRV.ReleaseAndGetAddressOf()),
                          "Failed to create backbuffer copy SRV.");

            /*ComPtr<ID3D11DeviceContext> pDeviceContext;
            pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());*/
        } else if ((std::strcmp("BackBuffer_Resolved", name) == 0) || (std::strcmp("BackBufferCopy", name) == 0)) {
            pGameBackBufferResolved = pTexture;
        } else if (std::strcmp("VideoEncode", name) == 0) {
            const ComPtr<ID3D11Texture2D>& pExportTexture = pTexture;

            D3D11_TEXTURE2D_DESC desc;
            pExportTexture->GetDesc(&desc);

            LOG(LL_NFO, "Detour_CreateTexture: fmt:", conv_dxgi_format_to_string(desc.Format), " w:", desc.Width,
                " h:", desc.Height);
            std::lock_guard<std::mutex> sessionLock(mxSession);
            LOG_CALL(LL_DBG, ::exportContext.reset());
            LOG_CALL(LL_DBG, encodingSession.reset());
            try {
                LOG(LL_NFO, "Creating session...");

                if (config::auto_reload_config) {
                    LOG_CALL(LL_DBG, config::reload());
                }

                encodingSession.reset(new Encoder::Session());
                NOT_NULL(encodingSession, "Could not create the session");
                ::exportContext.reset(new ExportContext());
                NOT_NULL(::exportContext, "Could not create export context");
                ::exportContext->pSwapChain = mainSwapChain;
                ::exportContext->pExportRenderTarget = pExportTexture;

                pExportTexture->GetDevice(::exportContext->pDevice.GetAddressOf());
                ::exportContext->pDevice->GetImmediateContext(::exportContext->pDeviceContext.GetAddressOf());
            } catch (std::exception& ex) {
                LOG(LL_ERR, ex.what());
                LOG_CALL(LL_DBG, encodingSession.reset());
                LOG_CALL(LL_DBG, ::exportContext.reset());
            }
        }
    }

    return result;
}

static void prepareDeferredContext(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext) {
    REQUIRE(pDevice->CreateDeferredContext(0, pDContext.GetAddressOf()), "Failed to create deferred context");
    REQUIRE(pDevice->CreateVertexShader(VSFullScreen::g_main, sizeof(VSFullScreen::g_main), NULL,
                                        pVSFullScreen.GetAddressOf()),
            "Failed to create g_VSFullScreen vertex shader");
    REQUIRE(pDevice->CreatePixelShader(PSAccumulate::g_main, sizeof(PSAccumulate::g_main), NULL,
                                       pPSAccumulate.GetAddressOf()),
            "Failed to create pixel shader");
    REQUIRE(pDevice->CreatePixelShader(PSDivide::g_main, sizeof(PSDivide::g_main), NULL, pPSDivide.GetAddressOf()),
            "Failed to create pixel shader");

    D3D11_BUFFER_DESC clipBufDesc;
    clipBufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
    clipBufDesc.ByteWidth = sizeof(PSConstantBuffer);
    clipBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
    clipBufDesc.MiscFlags = 0;
    clipBufDesc.StructureByteStride = 0;
    clipBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;

    REQUIRE(pDevice->CreateBuffer(&clipBufDesc, NULL, pDivideConstantBuffer.GetAddressOf()),
            "Failed to create constant buffer for pixel shader");

    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.MinLOD = -FLT_MAX;
    samplerDesc.MaxLOD = FLT_MAX;

    REQUIRE(pDevice->CreateSamplerState(&samplerDesc, pPSSampler.GetAddressOf()), "Failed to create sampler state");

    D3D11_RASTERIZER_DESC rasterStateDesc;
    rasterStateDesc.AntialiasedLineEnable = FALSE;
    rasterStateDesc.CullMode = D3D11_CULL_FRONT;
    rasterStateDesc.DepthClipEnable = FALSE;
    rasterStateDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
    rasterStateDesc.MultisampleEnable = FALSE;
    rasterStateDesc.ScissorEnable = FALSE;
    REQUIRE(pDevice->CreateRasterizerState(&rasterStateDesc, pRasterState.GetAddressOf()),
            "Failed to create raster state");

    D3D11_BLEND_DESC blendDesc;
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    REQUIRE(pDevice->CreateBlendState(&blendDesc, pAccBlendState.GetAddressOf()),
            "Failed to create accumulation blend state");
}

static void drawAdditive(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext,
                         ComPtr<ID3D11Texture2D> pSource) {
    D3D11_TEXTURE2D_DESC desc;
    pSource->GetDesc(&desc);

    LOG_CALL(LL_DBG, pDContext->ClearState());
    LOG_CALL(LL_DBG, pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0));
    LOG_CALL(LL_DBG, pDContext->IASetInputLayout(NULL));
    LOG_CALL(LL_DBG, pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP));

    LOG_CALL(LL_DBG, pDContext->VSSetShader(pVSFullScreen.Get(), NULL, 0));
    LOG_CALL(LL_DBG, pDContext->PSSetSamplers(0, 1, pPSSampler.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->PSSetShader(pPSAccumulate.Get(), NULL, 0));
    LOG_CALL(LL_DBG, pDContext->RSSetState(pRasterState.Get()));

    float factors[4] = {0, 0, 0, 0};

    LOG_CALL(LL_DBG, pDContext->OMSetBlendState(pAccBlendState.Get(), factors, 0xFFFFFFFF));

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = desc.Width;
    viewport.Height = desc.Height;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    LOG_CALL(LL_DBG, pDContext->RSSetViewports(1, &viewport));

    LOG_CALL(LL_DBG, pDContext->CopyResource(pSourceSRVTexture.Get(), pSource.Get()));

    LOG_CALL(LL_DBG, pDContext->PSSetShaderResources(0, 1, pSourceSRV.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->OMSetRenderTargets(1, pRTVAccBuffer.GetAddressOf(), nullptr));
    if (::exportContext->accCount == 0) {
        float color[4] = {0, 0, 0, 1};
        LOG_CALL(LL_DBG, pDContext->ClearRenderTargetView(pRTVAccBuffer.Get(), color));
    }

    LOG_CALL(LL_DBG, pDContext->Draw(4, 0));

    ComPtr<ID3D11CommandList> pCmdList;
    LOG_CALL(LL_DBG, pDContext->FinishCommandList(FALSE, pCmdList.GetAddressOf()));
    LOG_CALL(LL_DBG, pContext->ExecuteCommandList(pCmdList.Get(), TRUE));

    ::exportContext->accCount++;
}

static ComPtr<ID3D11Texture2D> divideBuffer(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext,
                                            uint32_t k) {

    D3D11_TEXTURE2D_DESC desc;
    pMotionBlurFinalBuffer->GetDesc(&desc);

    LOG_CALL(LL_DBG, pDContext->ClearState());
    LOG_CALL(LL_DBG, pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0));
    LOG_CALL(LL_DBG, pDContext->IASetInputLayout(NULL));
    LOG_CALL(LL_DBG, pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP));

    LOG_CALL(LL_DBG, pDContext->VSSetShader(pVSFullScreen.Get(), NULL, 0));
    LOG_CALL(LL_DBG, pDContext->PSSetShader(pPSDivide.Get(), NULL, 0));

    D3D11_MAPPED_SUBRESOURCE mapped;
    REQUIRE(pDContext->Map(pDivideConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
            "Failed to map divide shader constant buffer");
    ;
    float* floats = (float*)mapped.pData;
    floats[0] = static_cast<float>(k);
    /*floats[0] = 1.0f;*/
    LOG_CALL(LL_DBG, pDContext->Unmap(pDivideConstantBuffer.Get(), 0));

    LOG_CALL(LL_DBG, pDContext->PSSetConstantBuffers(0, 1, pDivideConstantBuffer.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->PSSetShaderResources(0, 1, pSRVAccBuffer.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->RSSetState(pRasterState.Get()));

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = desc.Width;
    viewport.Height = desc.Height;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    LOG_CALL(LL_DBG, pDContext->RSSetViewports(1, &viewport));

    LOG_CALL(LL_DBG, pDContext->OMSetRenderTargets(1, pRTVBlurBuffer.GetAddressOf(), NULL));
    float color[4] = {0, 0, 0, 1};
    LOG_CALL(LL_DBG, pDContext->ClearRenderTargetView(pRTVBlurBuffer.Get(), color));

    LOG_CALL(LL_DBG, pDContext->Draw(4, 0));

    ComPtr<ID3D11CommandList> pCmdList;
    LOG_CALL(LL_DBG, pDContext->FinishCommandList(FALSE, pCmdList.GetAddressOf()));
    LOG_CALL(LL_DBG, pContext->ExecuteCommandList(pCmdList.Get(), TRUE));

    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;

    ComPtr<ID3D11Texture2D> ret;
    REQUIRE(pDevice->CreateTexture2D(&desc, NULL, ret.GetAddressOf()),
            "Failed to create copy of motion blur dest texture");
    LOG_CALL(LL_DBG, pContext->CopyResource(ret.Get(), pMotionBlurFinalBuffer.Get()));

    return ret;
}