// script.cpp : Defines the exported functions for the DLL application.

#pragma comment(lib, "d3d11.lib")

#include "script.h"
#include "MFUtility.h"
#include "encoder.h"
#include "hookdefs.h"
#include "logger.h"
#include "reshade.hpp"
#include "stdafx.h"
#include "util.h"
#include "yara-helper.h"
#include <mferror.h>

#include "yara-patterns.h"
#include <DirectXTex.h>
#include <cwctype>
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
void* pCtxLinearizeBuffer = nullptr;
bool isCustomFrameRateSupported = false;
std::unique_ptr<Encoder::Session> encodingSession;
std::mutex mxSession;
std::mutex mxOnPresent;
std::thread::id mainThreadId;
ComPtr<IDXGISwapChain> mainSwapChain;
ComPtr<IDXGIFactory> pDxgiFactory;
ComPtr<ID3D11DeviceContext> pDContext;
ComPtr<ID3D11Texture2D> pMotionBlurAccBuffer;
ComPtr<ID3D11Texture2D> pMotionBlurFinalBuffer;
ComPtr<ID3D11VertexShader> pVsFullScreen;
ComPtr<ID3D11PixelShader> pPsAccumulate;
ComPtr<ID3D11PixelShader> pPsDivide;
ComPtr<ID3D11Buffer> pDivideConstantBuffer;
ComPtr<ID3D11RasterizerState> pRasterState;
ComPtr<ID3D11SamplerState> pPsSampler;
ComPtr<ID3D11RenderTargetView> pRtvAccBuffer;
ComPtr<ID3D11RenderTargetView> pRtvBlurBuffer;
ComPtr<ID3D11ShaderResourceView> pSrvAccBuffer;
ComPtr<ID3D11Texture2D> pSourceSrvTexture;
ComPtr<ID3D11ShaderResourceView> pSourceSrv;
ComPtr<ID3D11BlendState> pAccBlendState;

struct ExportContext {
    ExportContext() {
        PRE();
        POST();
    }
    ~ExportContext() {
        PRE();
        POST();
    }

    ExportContext(const ExportContext& other) = delete;
    ExportContext& operator=(const ExportContext& other) = delete;
    ExportContext(ExportContext&& other) = delete;
    ExportContext& operator=(ExportContext&& other) = delete;

    bool is_audio_export_disabled = false;
    ComPtr<ID3D11Texture2D> p_export_render_target;
    ComPtr<ID3D11DeviceContext> p_device_context;
    ComPtr<ID3D11Device> p_device;
    ComPtr<IDXGISwapChain> p_swap_chain;
    ComPtr<IMFMediaType> video_media_type;
    int acc_count = 0;
    int total_frame_num = 0;
};

std::unique_ptr<ExportContext> exportContext;
std::unique_ptr<YaraHelper> pYaraHelper;

} // namespace

void eve::OnInitDevice(reshade::api::device* device) {
    static bool deferredContextPrepared = false;
    if (!deferredContextPrepared) {
        eve::PrepareDeferredContext(reinterpret_cast<ID3D11Device*>(device->get_native()));
        deferredContextPrepared = true;
    }
}

void eve::OnInitSwapChain(reshade::api::swapchain* swapchain) {
    if (!mainSwapChain) {
        std::lock_guard onPresentLock(mxOnPresent);
        mainSwapChain = reinterpret_cast<IDXGISwapChain*>(swapchain->get_native());
    }
}

void eve::OnPresent(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain,
                    const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect,
                    uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects) {
    // if (!mainSwapChain) {
    //     std::lock_guard onPresentLock(mxOnPresent);
    //     mainSwapChain = reinterpret_cast<IDXGISwapChain*>(swapchain->get_native());
    // }
}

// HRESULT ExportHooks::D3D11CreateDeviceAndSwapChain::Implementation(
//     IDXGIAdapter* p_adapter, D3D_DRIVER_TYPE driver_type, HMODULE software, UINT flags,
//     const D3D_FEATURE_LEVEL* p_feature_levels, UINT feature_levels, UINT sdk_version,
//     const DXGI_SWAP_CHAIN_DESC* p_swap_chain_desc, IDXGISwapChain** pp_swap_chain, ID3D11Device** pp_device,
//     D3D_FEATURE_LEVEL* p_feature_level, ID3D11DeviceContext** pp_immediate_context) {
//     PRE();
//     const HRESULT result =
//         OriginalFunc(p_adapter, driver_type, software, flags, p_feature_levels, feature_levels, sdk_version,
//                      p_swap_chain_desc, pp_swap_chain, pp_device, p_feature_level, pp_immediate_context);
//     // if (SUCCEEDED(result) && pp_swap_chain && *pp_swap_chain) {
//     //     try {
//     //         // mainSwapChain = *pp_swap_chain;
//     //         PERFORM_MEMBER_HOOK_REQUIRED(IDXGISwapChain, Present, *pp_swap_chain);
//     //         PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, Draw, *pp_immediate_context);
//     //         PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, DrawIndexed, *pp_immediate_context);
//     //         PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, OMSetRenderTargets, *pp_immediate_context);
//     //     } catch (...) {
//     //         LOG(LL_ERR, "Hooking IDXGISwapChain functions failed");
//     //     }
//     // }
//     POST();
//     return result;
// }

// HRESULT IDXGISwapChainHooks::Present::Implementation(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags) {
//
//     ID3D11Device* pDevice;
//     pThis->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice));
//
//     ComPtr<ID3D11DeviceContext> pDeviceContext;
//     pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
//
//     PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, Draw, pDeviceContext.Get());
//     PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, DrawIndexed, pDeviceContext.Get());
//     PERFORM_MEMBER_HOOK_REQUIRED(ID3D11DeviceContext, OMSetRenderTargets, pDeviceContext.Get());
//
//     return OriginalFunc(pThis, SyncInterval, Flags);
// }

// void PerformD3D11Hooks() {
//     WNDCLASSEX windowClass;
//     windowClass.cbSize = sizeof(WNDCLASSEX);
//     windowClass.style = CS_HREDRAW | CS_VREDRAW;
//     windowClass.lpfnWndProc = DefWindowProc;
//     windowClass.cbClsExtra = 0;
//     windowClass.cbWndExtra = 0;
//     windowClass.hInstance = GetModuleHandle(NULL);
//     windowClass.hIcon = NULL;
//     windowClass.hCursor = NULL;
//     windowClass.hbrBackground = NULL;
//     windowClass.lpszMenuName = NULL;
//     windowClass.lpszClassName = "EVE";
//     windowClass.hIconSm = NULL;
//
//     ::RegisterClassEx(&windowClass);
//
//     HWND window = ::CreateWindow(windowClass.lpszClassName, "EVE Hook Window", WS_OVERLAPPED, 0, 0, 100, 100, NULL,
//                                  NULL, windowClass.hInstance, NULL);
//
//     NOT_NULL(window, "Failed to create window");
//
//     // Create D3D11 Device and SwapChain
//     D3D_FEATURE_LEVEL featureLevel;
//     const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0};
//
//     DXGI_RATIONAL refreshRate;
//     refreshRate.Numerator = 60;
//     refreshRate.Denominator = 1;
//
//     DXGI_MODE_DESC bufferDesc;
//     bufferDesc.Width = 100;
//     bufferDesc.Height = 100;
//     bufferDesc.RefreshRate = refreshRate;
//     bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//     bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
//     bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
//
//     DXGI_SAMPLE_DESC sampleDesc;
//     sampleDesc.Count = 1;
//     sampleDesc.Quality = 0;
//
//     DXGI_SWAP_CHAIN_DESC swapChainDesc;
//     swapChainDesc.BufferDesc = bufferDesc;
//     swapChainDesc.SampleDesc = sampleDesc;
//     swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
//     swapChainDesc.BufferCount = 1;
//     swapChainDesc.OutputWindow = window;
//     swapChainDesc.Windowed = 1;
//     swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
//     swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
//
//     ComPtr<IDXGISwapChain> pSwapChain;
//     ComPtr<ID3D11Device> pDevice;
//     ComPtr<ID3D11DeviceContext> pDeviceContext;
//
//     REQUIRE(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels,
//                                           ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &swapChainDesc,
//                                           pSwapChain.GetAddressOf(), pDevice.GetAddressOf(), &featureLevel,
//                                           pDeviceContext.GetAddressOf()),
//             "Failed to create D3D11 Device and SwapChain");
//
//     LOG(LL_NFO, "D3D11 Device and SwapChain created");
//
//     if (pSwapChain && pDeviceContext) {
//         try {
//             PERFORM_MEMBER_HOOK_REQUIRED(IDXGISwapChain, Present, pSwapChain.Get());
//         } catch (...) {
//             LOG(LL_ERR, "Hooking IDXGISwapChain functions failed");
//         }
//     }
//
//     pSwapChain = nullptr;
//     pDeviceContext = nullptr;
//     pDevice = nullptr;
//
//     DestroyWindow(window);
// }

// void eve::PresentCallback(void* p_swap_chain) {}

void eve::initialize() {
    PRE();

    try {
        mainThreadId = std::this_thread::get_id();

        LOG(LL_NFO, "Initializing Media Foundation hook");
        PERFORM_NAMED_IMPORT_HOOK_REQUIRED("mfreadwrite.dll", MFCreateSinkWriterFromURL);

        pYaraHelper.reset(new YaraHelper());
        pYaraHelper->Initialize();

        MODULEINFO info;
        GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &info, sizeof(info));
        LOG(LL_NFO, "Image base:", ((void*)info.lpBaseOfDll));

        uint64_t pGetRenderTimeBase = NULL;
        uint64_t pCreateTexture = NULL;
        uint64_t pCreateThread = NULL;
        pYaraHelper->AddEntry("get_render_time_base_function", yara_get_render_time_base_function, &pGetRenderTimeBase);
        pYaraHelper->AddEntry("create_thread_function", yara_create_thread_function, &pCreateThread);
        pYaraHelper->AddEntry("create_texture_function", yara_create_texture_function, &pCreateTexture);
        pYaraHelper->PerformScan();

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
            } else {
                LOG(LL_ERR, "Could not find the address for CreateTexture function.");
            }

            if (pCreateThread) {
                PERFORM_X64_HOOK_REQUIRED(CreateThread, pCreateThread);
            } else {
                LOG(LL_ERR, "Could not find the address for CreateThread function.");
            }
        } catch (std::exception& ex) {
            LOG(LL_ERR, ex.what());
        }
    } catch (std::exception& ex) {
        // TODO cleanup
        POST();
        throw ex;
    }
    POST();
}

void eve::ScriptMain() {
    PRE();
    LOG(LL_NFO, "Starting main loop");
    while (true) {
        WAIT(0);
    }
}

void eve::OnBindRenderTargets(reshade::api::command_list* cmd_list, uint32_t count,
                              const reshade::api::resource_view* render_targets,
                              reshade::api::resource_view depth_stencil) {

    // PRE();
    if (::exportContext) {
        for (uint32_t i = 0; i < count; i++) {
            if (render_targets[i].handle) {
                ComPtr<ID3D11Resource> pResource;
                ComPtr<ID3D11Texture2D> pTexture2D;
                reinterpret_cast<ID3D11RenderTargetView*>(render_targets[i].handle)
                    ->GetResource(pResource.GetAddressOf());
                if (SUCCEEDED(pResource.As(&pTexture2D))) {
                    if (pTexture2D.Get() == pGameDepthBufferQuarterLinear.Get()) {
                        LOG(LL_DBG, " i:", i, " num:", count, " dsv:", depth_stencil.handle);
                        pCtxLinearizeBuffer = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
                    }
                }
            }
        }
    }

    ComPtr<ID3D11Resource> pRTVTexture;
    if (render_targets && (render_targets[0].handle)) {
        reinterpret_cast<ID3D11RenderTargetView*>(render_targets[0].handle)->GetResource(pRTVTexture.GetAddressOf());
    }

    if (::exportContext != nullptr && ::exportContext->p_export_render_target != nullptr &&
        (::exportContext->p_export_render_target == pRTVTexture)) {
        // LOG(LL_DBG, "Exporting frame");
        //  Time to capture rendered frame
        try {
            ComPtr<ID3D11Device> pDevice;
            reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->GetDevice(pDevice.GetAddressOf());

            ComPtr<ID3D11Texture2D> pDepthBufferCopy = nullptr;
            ComPtr<ID3D11Texture2D> pBackBufferCopy = nullptr;

            if (config::export_openexr) {
                TRY([&] {
                    {
                        D3D11_TEXTURE2D_DESC desc;
                        pLinearDepthTexture->GetDesc(&desc);

                        desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
                        desc.BindFlags = 0;
                        desc.MiscFlags = 0;
                        desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

                        REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pDepthBufferCopy.GetAddressOf()),
                                "Failed to create depth buffer copy texture");

                        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                            ->CopyResource(pDepthBufferCopy.Get(), pLinearDepthTexture.Get());
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

                        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                            ->CopyResource(pBackBufferCopy.Get(), pGameBackBufferResolved.Get());
                    }
                    {
                        std::lock_guard<std::mutex> sessionLock(mxSession);
                        if ((encodingSession != nullptr) && (encodingSession->isCapturing)) {
                            encodingSession->enqueueEXRImage(
                                reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native()), pBackBufferCopy,
                                pDepthBufferCopy);
                        }
                    }
                });
            }

            ComPtr<ID3D11Texture2D> pSwapChainBuffer;
            REQUIRE(::exportContext->p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                                             reinterpret_cast<void**>(pSwapChainBuffer.GetAddressOf())),
                    "Failed to get swap chain's buffer");

            LOG_CALL(LL_DBG,
                     ::exportContext->p_swap_chain->Present(1, 0)); // IMPORTANT: This call makes ENB and ReShade
                                                                    // effects to be applied to the render target

            LOG_CALL(LL_DBG,
                     ::exportContext->p_swap_chain->Present(1, 0)); // IMPORTANT: This call makes ENB and ReShade
                                                                    // effects to be applied to the render target

            if (config::motion_blur_samples != 0) {
                const float current_shutter_position =
                    (::exportContext->total_frame_num % (config::motion_blur_samples + 1)) /
                    static_cast<float>(config::motion_blur_samples);

                if (current_shutter_position >= (1 - config::motion_blur_strength)) {
                    eve::drawAdditive(pDevice, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native()),
                                      pSwapChainBuffer);
                }
            } else {
                // Trick to use the same buffers for when not using motion blur
                ::exportContext->acc_count = 0;
                eve::drawAdditive(pDevice, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native()),
                                  pSwapChainBuffer);
                ::exportContext->acc_count = 1;
                ::exportContext->total_frame_num = 1;
            }

            if ((::exportContext->total_frame_num % (::config::motion_blur_samples + 1)) ==
                config::motion_blur_samples) {

                const ComPtr<ID3D11Texture2D> result =
                    eve::divideBuffer(pDevice, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native()),
                                      ::exportContext->acc_count);
                ::exportContext->acc_count = 0;

                // Save image as dds file
                // ScratchImage image;
                // if (SUCCEEDED(CaptureTexture(pDevice.Get(),
                // reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native()), pSwapChainBuffer.Get(), image))) {
                //    const std::wstring path = L"C:\\ExtendedVideoExport\\test.dds";
                //    if (FAILED(SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                //                             DDS_FLAGS_NONE, path.c_str()))) {
                //        LOG(LL_ERR, "Failed to save image to PNG file.");
                //    }
                //}

                D3D11_MAPPED_SUBRESOURCE mapped;

                REQUIRE(reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                            ->Map(result.Get(), 0, D3D11_MAP_READ, 0, &mapped),
                        "Failed to capture swapbuffer.");
                {
                    std::lock_guard sessionLock(mxSession);
                    if ((encodingSession != nullptr) && (encodingSession->isCapturing)) {
                        REQUIRE(encodingSession->enqueueVideoFrame(mapped), "Failed to enqueue frame.");
                    }
                }
                reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->Unmap(result.Get(), 0);
            }
            ::exportContext->total_frame_num++;
        } catch (std::exception&) {
            LOG(LL_ERR, "Reading video frame from D3D Device failed.");
            LOG_CALL(LL_DBG, encodingSession.reset());
            LOG_CALL(LL_DBG, ::exportContext.reset());
        }
    }
    // POST();
}

HRESULT ImportHooks::MFCreateSinkWriterFromURL::Implementation(LPCWSTR pwszOutputURL, IMFByteStream* pByteStream,
                                                               IMFAttributes* pAttributes,
                                                               IMFSinkWriter** ppSinkWriter) {
    PRE();
    const HRESULT result = OriginalFunc(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
    if (SUCCEEDED(result)) {
        try {
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, AddStream, *ppSinkWriter);
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, SetInputMediaType, *ppSinkWriter);
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, WriteSample, *ppSinkWriter);
            PERFORM_MEMBER_HOOK_REQUIRED(IMFSinkWriter, Finalize, *ppSinkWriter);
        } catch (...) {
            LOG(LL_ERR, "Hooking IMFSinkWriter functions failed");
        }
    }
    POST();
    return result;
}

HRESULT IMFSinkWriterHooks::AddStream::Implementation(IMFSinkWriter* pThis, IMFMediaType* pTargetMediaType,
                                                      DWORD* pdwStreamIndex) {
    PRE();
    LOG(LL_NFO, "IMFSinkWriter::AddStream: ", GetMediaTypeDescription(pTargetMediaType).c_str());
    POST();
    return OriginalFunc(pThis, pTargetMediaType, pdwStreamIndex);
}

void CreateMotionBlurBuffers(ComPtr<ID3D11Device> p_device, uint32_t width, uint32_t height) {
    D3D11_TEXTURE2D_DESC motionBlurBufferDesc;
    motionBlurBufferDesc.Width = width;
    motionBlurBufferDesc.Height = height;
    motionBlurBufferDesc.MipLevels = 1;
    motionBlurBufferDesc.ArraySize = 1;
    motionBlurBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    motionBlurBufferDesc.SampleDesc.Count = 1;
    motionBlurBufferDesc.SampleDesc.Quality = 0;
    motionBlurBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    motionBlurBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    motionBlurBufferDesc.CPUAccessFlags = 0;
    motionBlurBufferDesc.MiscFlags = 0;

    D3D11_TEXTURE2D_DESC accBufDesc = motionBlurBufferDesc;
    accBufDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    accBufDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    accBufDesc.CPUAccessFlags = 0;
    accBufDesc.MiscFlags = 0;
    accBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
    accBufDesc.MipLevels = 1;
    accBufDesc.ArraySize = 1;
    accBufDesc.SampleDesc.Count = 1;
    accBufDesc.SampleDesc.Quality = 0;

    LOG_IF_FAILED(p_device->CreateTexture2D(&accBufDesc, NULL, pMotionBlurAccBuffer.ReleaseAndGetAddressOf()),
                  "Failed to create accumulation buffer texture");

    D3D11_TEXTURE2D_DESC mbBufferDesc = accBufDesc;
    mbBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    mbBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    LOG_IF_FAILED(p_device->CreateTexture2D(&mbBufferDesc, NULL, pMotionBlurFinalBuffer.ReleaseAndGetAddressOf()),
                  "Failed to create motion blur buffer texture");

    D3D11_RENDER_TARGET_VIEW_DESC accBufRTVDesc;
    accBufRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    accBufRTVDesc.Texture2D.MipSlice = 0;
    accBufRTVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    LOG_IF_FAILED(p_device->CreateRenderTargetView(pMotionBlurAccBuffer.Get(), &accBufRTVDesc,
                                                   pRtvAccBuffer.ReleaseAndGetAddressOf()),
                  "Failed to create acc buffer RTV.");

    D3D11_RENDER_TARGET_VIEW_DESC mbBufferRTVDesc;
    mbBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    mbBufferRTVDesc.Texture2D.MipSlice = 0;
    mbBufferRTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    LOG_IF_FAILED(p_device->CreateRenderTargetView(pMotionBlurFinalBuffer.Get(), &mbBufferRTVDesc,
                                                   pRtvBlurBuffer.ReleaseAndGetAddressOf()),
                  "Failed to create blur buffer RTV.");

    D3D11_SHADER_RESOURCE_VIEW_DESC accBufSRVDesc;
    accBufSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    accBufSRVDesc.Texture2D.MipLevels = 1;
    accBufSRVDesc.Texture2D.MostDetailedMip = 0;
    accBufSRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    LOG_IF_FAILED(p_device->CreateShaderResourceView(pMotionBlurAccBuffer.Get(), &accBufSRVDesc,
                                                     pSrvAccBuffer.ReleaseAndGetAddressOf()),
                  "Failed to create blur buffer SRV.");

    D3D11_TEXTURE2D_DESC sourceSRVTextureDesc = motionBlurBufferDesc;
    sourceSRVTextureDesc.CPUAccessFlags = 0;
    sourceSRVTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sourceSRVTextureDesc.MiscFlags = 0;
    sourceSRVTextureDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
    sourceSRVTextureDesc.ArraySize = 1;
    sourceSRVTextureDesc.SampleDesc.Count = 1;
    sourceSRVTextureDesc.SampleDesc.Quality = 0;
    sourceSRVTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sourceSRVTextureDesc.MipLevels = 1;

    REQUIRE(p_device->CreateTexture2D(&sourceSRVTextureDesc, NULL, pSourceSrvTexture.ReleaseAndGetAddressOf()),
            "Failed to create back buffer copy texture");

    D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
    sourceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sourceSRVDesc.Texture2D.MipLevels = 1;
    sourceSRVDesc.Texture2D.MostDetailedMip = 0;
    sourceSRVDesc.Format = sourceSRVTextureDesc.Format;

    LOG_IF_FAILED(p_device->CreateShaderResourceView(pSourceSrvTexture.Get(), &sourceSRVDesc,
                                                     pSourceSrv.ReleaseAndGetAddressOf()),
                  "Failed to create backbuffer copy SRV.");
}

HRESULT IMFSinkWriterHooks::SetInputMediaType::Implementation(IMFSinkWriter* pThis, DWORD dwStreamIndex,
                                                              IMFMediaType* pInputMediaType,
                                                              IMFAttributes* pEncodingParameters) {
    PRE();
    LOG(LL_NFO, "IMFSinkWriter::SetInputMediaType: ", GetMediaTypeDescription(pInputMediaType).c_str());

    GUID majorType;
    if (SUCCEEDED(pInputMediaType->GetMajorType(&majorType))) {
        if (IsEqualGUID(majorType, MFMediaType_Video)) {
            ::exportContext->video_media_type = pInputMediaType;
        } else if (IsEqualGUID(majorType, MFMediaType_Audio)) {
            try {
                std::lock_guard<std::mutex> sessionLock(mxSession);
                UINT width, height, fps_num, fps_den;
                MFGetAttribute2UINT32asUINT64(::exportContext->video_media_type.Get(), MF_MT_FRAME_SIZE, &width,
                                              &height);
                MFGetAttributeRatio(::exportContext->video_media_type.Get(), MF_MT_FRAME_RATE, &fps_num, &fps_den);

                GUID pixelFormat;
                ::exportContext->video_media_type->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

                if (isCustomFrameRateSupported) {
                    auto fps = config::fps;
                    fps_num = fps.first;
                    fps_den = fps.second;

                    float gameFrameRate =
                        (static_cast<float>(fps.first) * (static_cast<float>(config::motion_blur_samples) + 1) /
                         static_cast<float>(fps.second));
                    if (gameFrameRate > 60.0f) {
                        LOG(LL_NON, "fps * (motion_blur_samples + 1) > 60.0!!!");
                        LOG(LL_NON, "Audio export will be disabled!!!");
                        ::exportContext->is_audio_export_disabled = true;
                    }
                }

                UINT32 blockAlignment, numChannels, sampleRate, bitsPerSample;
                GUID subType;

                pInputMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
                pInputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
                pInputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
                pInputMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
                pInputMediaType->GetGUID(MF_MT_SUBTYPE, &subType);

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

                DXGI_SWAP_CHAIN_DESC desc;
                ::exportContext->p_swap_chain->GetDesc(&desc);

                const auto exportWidth = desc.BufferDesc.Width;
                const auto exportHeight = desc.BufferDesc.Height;

                // Get pBackBufferResolved dimensions for OpenEXR export
                D3D11_TEXTURE2D_DESC backBufferCopyDesc;
                pGameBackBufferResolved->GetDesc(&backBufferCopyDesc);
                const auto openExrWidth = backBufferCopyDesc.Width;
                const auto openExrHeight = backBufferCopyDesc.Height;

                LOG(LL_DBG, "Video Export Resolution: ", exportWidth, "x", exportHeight);
                LOG(LL_DBG, "OpenEXR Export Resolution: ", openExrWidth, "x", openExrHeight);

                CreateMotionBlurBuffers(::exportContext->p_device, exportWidth, exportHeight);

                REQUIRE(encodingSession->createContext(
                            config::encoder_config, std::wstring(filename.begin(), filename.end()), exportWidth,
                            exportHeight, "rgba", fps_num, fps_den, numChannels, sampleRate, "s16", blockAlignment,
                            config::export_openexr, openExrWidth, openExrHeight),
                        "Failed to create encoding context.");
            } catch (std::exception& ex) {
                LOG(LL_ERR, ex.what());
                LOG_CALL(LL_DBG, encodingSession.reset());
                LOG_CALL(LL_DBG, ::exportContext.reset());
                return MF_E_INVALIDMEDIATYPE;
            }
        }
    }

    POST();
    return OriginalFunc(pThis, dwStreamIndex, pInputMediaType, pEncodingParameters);
}

HRESULT IMFSinkWriterHooks::WriteSample::Implementation(IMFSinkWriter* pThis, DWORD dwStreamIndex, IMFSample* pSample) {
    std::lock_guard sessionLock(mxSession);

    if ((encodingSession) && (dwStreamIndex == 1) && (!::exportContext->is_audio_export_disabled)) {

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
        }
    }
    return S_OK;
}

HRESULT IMFSinkWriterHooks::Finalize::Implementation(IMFSinkWriter* pThis) {
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

void eve::finalize() {
    PRE();
    POST();
}

bool eve::OnDraw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count,
                 uint32_t first_vertex, uint32_t first_instance) {
    thread_local bool is_recursive_call = false;

    if (is_recursive_call) {
        return true;
    }

    if (pCtxLinearizeBuffer == cmd_list) {
        pCtxLinearizeBuffer = nullptr;

        ComPtr<ID3D11Device> pDevice;
        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->GetDevice(pDevice.GetAddressOf());

        ComPtr<ID3D11RenderTargetView> pCurrentRTV;
        LOG_CALL(LL_DBG, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                             ->OMGetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        pCurrentRTV->GetDesc(&rtvDesc);
        LOG_CALL(LL_DBG, pDevice->CreateRenderTargetView(pLinearDepthTexture.Get(), &rtvDesc,
                                                         pCurrentRTV.ReleaseAndGetAddressOf()));
        LOG_CALL(LL_DBG, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                             ->OMSetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));

        D3D11_TEXTURE2D_DESC ldtDesc;
        pLinearDepthTexture->GetDesc(&ldtDesc);

        D3D11_VIEWPORT viewport;
        viewport.Width = static_cast<float>(ldtDesc.Width);
        viewport.Height = static_cast<float>(ldtDesc.Height);
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;
        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->RSSetViewports(1, &viewport);
        D3D11_RECT rect;
        rect.left = rect.top = 0;
        rect.right = static_cast<LONG>(ldtDesc.Width);
        rect.bottom = static_cast<LONG>(ldtDesc.Height);
        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->RSSetScissorRects(1, &rect);

        is_recursive_call = true;
        LOG_CALL(LL_TRC, cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance));
        is_recursive_call = false;
    }

    return true;
}

bool eve::OnDrawIndexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count,
                        uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    thread_local bool is_recursive_call = false;

    if (is_recursive_call) {
        return true;
    }

    if (pCtxLinearizeBuffer == cmd_list) {
        pCtxLinearizeBuffer = nullptr;

        ComPtr<ID3D11Device> pDevice;
        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->GetDevice(pDevice.GetAddressOf());

        ComPtr<ID3D11RenderTargetView> pCurrentRTV;
        LOG_CALL(LL_DBG, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                             ->OMGetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        pCurrentRTV->GetDesc(&rtvDesc);
        LOG_CALL(LL_DBG, pDevice->CreateRenderTargetView(pLinearDepthTexture.Get(), &rtvDesc,
                                                         pCurrentRTV.ReleaseAndGetAddressOf()));
        LOG_CALL(LL_DBG, reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())
                             ->OMSetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));

        D3D11_TEXTURE2D_DESC ldtDesc;
        pLinearDepthTexture->GetDesc(&ldtDesc);

        D3D11_VIEWPORT viewport;
        viewport.Width = static_cast<float>(ldtDesc.Width);
        viewport.Height = static_cast<float>(ldtDesc.Height);
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;
        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->RSSetViewports(1, &viewport);
        D3D11_RECT rect;
        rect.left = rect.top = 0;
        rect.right = static_cast<LONG>(ldtDesc.Width);
        rect.bottom = static_cast<LONG>(ldtDesc.Height);
        reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native())->RSSetScissorRects(1, &rect);

        is_recursive_call = true;
        LOG_CALL(LL_TRC,
                 cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance));
        is_recursive_call = false;
    }

    return true;
}

HANDLE GameHooks::CreateThread::Implementation(void* pFunc, void* pParams, int32_t r8d, int32_t r9d, void* rsp20,
                                               int32_t rsp28, char* name) {
    PRE();
    static bool d3dHooked = false;
    // if (!d3dHooked) {
    // PerformD3D11Hooks();
    // d3dHooked = true;
    //}
    void* result = GameHooks::CreateThread::OriginalFunc(pFunc, pParams, r8d, r9d, rsp20, rsp28, name);
    LOG(LL_TRC, "CreateThread:", " pFunc:", pFunc, " pParams:", pParams, " r8d:", Logger::hex(r8d, 4),
        " r9d:", Logger::hex(r9d, 4), " rsp20:", rsp20, " rsp28:", rsp28, " name:", name ? name : "<NULL>");
    POST();
    return result;
}

float GameHooks::GetRenderTimeBase::Implementation(int64_t choice) {
    PRE();
    const std::pair<int32_t, int32_t> fps = config::fps;
    const float result = 1000.0f * static_cast<float>(fps.second) /
                         (static_cast<float>(fps.first) * (static_cast<float>(config::motion_blur_samples) + 1));
    // float result = 1000.0f / 60.0f;
    LOG(LL_NFO, "Time step: ", result);
    POST();
    return result;
}

void AutoReloadConfig() {
    if (config::auto_reload_config) {
        LOG_CALL(LL_DBG, config::reload());
    }
}

void CreateNewExportContext() {
    encodingSession.reset(new Encoder::Session());
    NOT_NULL(encodingSession, "Could not create the session");
    ::exportContext.reset(new ExportContext());
    NOT_NULL(::exportContext, "Could not create export context");
}

void* GameHooks::CreateTexture::Implementation(void* rcx, char* name, const uint32_t r8d, uint32_t width,
                                               uint32_t height, const uint32_t format, void* rsp30) {
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

    LOG(LL_DBG, "CreateTexture:", " rcx:", rcx, " name:", name ? name : "<NULL>", " r8d:", Logger::hex(r8d, 8),
        " width:", width, " height:", height, " fmt:", conv_dxgi_format_to_string(fmt), " result:", result,
        " *r+0:", vresult ? *(vresult + 0) : "<NULL>", " *r+1:", vresult ? *(vresult + 1) : "<NULL>",
        " *r+2:", vresult ? *(vresult + 2) : "<NULL>", " *r+3:", vresult ? *(vresult + 3) : "<NULL>",
        " *r+4:", vresult ? *(vresult + 4) : "<NULL>", " *r+5:", vresult ? *(vresult + 5) : "<NULL>",
        " *r+6:", vresult ? *(vresult + 6) : "<NULL>", " *r+7:", vresult ? *(vresult + 7) : "<NULL>",
        " *r+8:", vresult ? *(vresult + 8) : "<NULL>", " *r+9:", vresult ? *(vresult + 9) : "<NULL>",
        " *r+10:", vresult ? *(vresult + 10) : "<NULL>", " *r+11:", vresult ? *(vresult + 11) : "<NULL>",
        " *r+12:", vresult ? *(vresult + 12) : "<NULL>", " *r+13:", vresult ? *(vresult + 13) : "<NULL>",
        " *r+14:", vresult ? *(vresult + 14) : "<NULL>", " *r+15:", vresult ? *(vresult + 15) : "<NULL>");

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
            REQUIRE(pDevice->CreateTexture2D(&desc, nullptr, pStencilTexture.GetAddressOf()),
                    "Failed to create stencil texture");
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

        } else if ((std::strcmp("BackBuffer_Resolved", name) == 0) || (std::strcmp("BackBufferCopy", name) == 0)) {
            pGameBackBufferResolved = pTexture;
        } else if (std::strcmp("VideoEncode", name) == 0) {
            std::lock_guard<std::mutex> sessionLock(mxSession);
            const ComPtr<ID3D11Texture2D>& pExportTexture = pTexture;

            D3D11_TEXTURE2D_DESC desc;
            pExportTexture->GetDesc(&desc);

            LOG_CALL(LL_DBG, ::exportContext.reset());
            LOG_CALL(LL_DBG, encodingSession.reset());
            try {
                LOG(LL_NFO, "Creating session...");

                AutoReloadConfig();
                CreateNewExportContext();
                ::exportContext->p_swap_chain = mainSwapChain;
                ::exportContext->p_export_render_target = pExportTexture;

                pExportTexture->GetDevice(::exportContext->p_device.GetAddressOf());
                ::exportContext->p_device->GetImmediateContext(::exportContext->p_device_context.GetAddressOf());

            } catch (std::exception& ex) {
                LOG(LL_ERR, ex.what());
                LOG_CALL(LL_DBG, encodingSession.reset());
                LOG_CALL(LL_DBG, ::exportContext.reset());
            }
        }
    }

    return result;
}

void eve::PrepareDeferredContext(ID3D11Device* p_device) {
    REQUIRE(p_device->CreateDeferredContext(0, pDContext.GetAddressOf()), "Failed to create deferred context");
    REQUIRE(p_device->CreateVertexShader(VSFullScreen::g_main, sizeof(VSFullScreen::g_main), NULL,
                                         pVsFullScreen.GetAddressOf()),
            "Failed to create g_VSFullScreen vertex shader");
    REQUIRE(p_device->CreatePixelShader(PSAccumulate::g_main, sizeof(PSAccumulate::g_main), NULL,
                                        pPsAccumulate.GetAddressOf()),
            "Failed to create pixel shader");
    REQUIRE(p_device->CreatePixelShader(PSDivide::g_main, sizeof(PSDivide::g_main), NULL, pPsDivide.GetAddressOf()),
            "Failed to create pixel shader");

    D3D11_BUFFER_DESC clipBufDesc;
    clipBufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
    clipBufDesc.ByteWidth = sizeof(XMFLOAT4);
    clipBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
    clipBufDesc.MiscFlags = 0;
    clipBufDesc.StructureByteStride = 0;
    clipBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;

    REQUIRE(p_device->CreateBuffer(&clipBufDesc, NULL, pDivideConstantBuffer.GetAddressOf()),
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

    REQUIRE(p_device->CreateSamplerState(&samplerDesc, pPsSampler.GetAddressOf()), "Failed to create sampler state");

    D3D11_RASTERIZER_DESC rasterStateDesc;
    rasterStateDesc.AntialiasedLineEnable = FALSE;
    rasterStateDesc.CullMode = D3D11_CULL_FRONT;
    rasterStateDesc.DepthClipEnable = FALSE;
    rasterStateDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
    rasterStateDesc.MultisampleEnable = FALSE;
    rasterStateDesc.ScissorEnable = FALSE;
    REQUIRE(p_device->CreateRasterizerState(&rasterStateDesc, pRasterState.GetAddressOf()),
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

    REQUIRE(p_device->CreateBlendState(&blendDesc, pAccBlendState.GetAddressOf()),
            "Failed to create accumulation blend state");
}

void eve::drawAdditive(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext,
                       ComPtr<ID3D11Texture2D> pSource) {
    D3D11_TEXTURE2D_DESC desc;
    pSource->GetDesc(&desc);

    LOG_CALL(LL_DBG, pDContext->ClearState());
    LOG_CALL(LL_DBG, pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0));
    LOG_CALL(LL_DBG, pDContext->IASetInputLayout(NULL));
    LOG_CALL(LL_DBG, pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP));

    LOG_CALL(LL_DBG, pDContext->VSSetShader(pVsFullScreen.Get(), NULL, 0));
    LOG_CALL(LL_DBG, pDContext->PSSetSamplers(0, 1, pPsSampler.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->PSSetShader(pPsAccumulate.Get(), NULL, 0));
    LOG_CALL(LL_DBG, pDContext->RSSetState(pRasterState.Get()));

    float factors[4] = {0, 0, 0, 0};

    LOG_CALL(LL_DBG, pDContext->OMSetBlendState(pAccBlendState.Get(), factors, 0xFFFFFFFF));

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(desc.Width);
    viewport.Height = static_cast<float>(desc.Height);
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    LOG_CALL(LL_DBG, pDContext->RSSetViewports(1, &viewport));

    LOG_CALL(LL_DBG, pDContext->CopyResource(pSourceSrvTexture.Get(), pSource.Get()));

    LOG_CALL(LL_DBG, pDContext->PSSetShaderResources(0, 1, pSourceSrv.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->OMSetRenderTargets(1, pRtvAccBuffer.GetAddressOf(), nullptr));
    if (::exportContext->acc_count == 0) {
        float color[4] = {0, 0, 0, 1};
        LOG_CALL(LL_DBG, pDContext->ClearRenderTargetView(pRtvAccBuffer.Get(), color));
    }

    LOG_CALL(LL_DBG, pDContext->Draw(4, 0));

    ComPtr<ID3D11CommandList> pCmdList;
    LOG_CALL(LL_DBG, pDContext->FinishCommandList(FALSE, pCmdList.GetAddressOf()));
    LOG_CALL(LL_DBG, pContext->ExecuteCommandList(pCmdList.Get(), TRUE));

    ::exportContext->acc_count++;
}

ComPtr<ID3D11Texture2D> eve::divideBuffer(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext,
                                          uint32_t k) {

    D3D11_TEXTURE2D_DESC desc;
    pMotionBlurFinalBuffer->GetDesc(&desc);

    LOG_CALL(LL_DBG, pDContext->ClearState());
    LOG_CALL(LL_DBG, pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0));
    LOG_CALL(LL_DBG, pDContext->IASetInputLayout(NULL));
    LOG_CALL(LL_DBG, pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP));

    LOG_CALL(LL_DBG, pDContext->VSSetShader(pVsFullScreen.Get(), NULL, 0));
    LOG_CALL(LL_DBG, pDContext->PSSetShader(pPsDivide.Get(), NULL, 0));

    D3D11_MAPPED_SUBRESOURCE mapped;
    REQUIRE(pDContext->Map(pDivideConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
            "Failed to map divide shader constant buffer");
    ;
    const auto floats = static_cast<float*>(mapped.pData);
    floats[0] = static_cast<float>(k);
    /*floats[0] = 1.0f;*/
    LOG_CALL(LL_DBG, pDContext->Unmap(pDivideConstantBuffer.Get(), 0));

    LOG_CALL(LL_DBG, pDContext->PSSetConstantBuffers(0, 1, pDivideConstantBuffer.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->PSSetShaderResources(0, 1, pSrvAccBuffer.GetAddressOf()));
    LOG_CALL(LL_DBG, pDContext->RSSetState(pRasterState.Get()));

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(desc.Width);
    viewport.Height = static_cast<float>(desc.Height);
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    LOG_CALL(LL_DBG, pDContext->RSSetViewports(1, &viewport));

    LOG_CALL(LL_DBG, pDContext->OMSetRenderTargets(1, pRtvBlurBuffer.GetAddressOf(), NULL));
    constexpr float color[4] = {0, 0, 0, 1};
    LOG_CALL(LL_DBG, pDContext->ClearRenderTargetView(pRtvBlurBuffer.Get(), color));

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