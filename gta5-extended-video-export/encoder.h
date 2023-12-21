#pragma once

#include "SafeQueue.h"
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <future>
#include <mfidl.h>
#include <mutex>
#include <utility>
#include <valarray>
#include <vector>
#include <wrl.h>
#pragma warning(push, 0)
#include <VoukoderTypeLib_h.h>
#pragma warning(pop)

namespace Encoder {
class Session {
  public:
    Microsoft::WRL::ComPtr<IVoukoder> pVoukoder = nullptr;
    VKVIDEOFRAME videoFrame{};
    VKAUDIOCHUNK audioChunk{};
    int64_t videoPTS = 0;
    int64_t motionBlurPTS = 0;

    int64_t audioPTS = 0;

    bool isVideoFinished = false;
    bool isAudioFinished = false;
    bool isSessionFinished = false;
    bool isCapturing = false;
    bool isBeingDeleted = false;

    std::mutex mxVoukoderContext;
    std::condition_variable cvVoukoderContext;
    // bool isVoukoderContextCreated = false;

    std::mutex mxEndSession;
    std::condition_variable cvEndSession;

    struct frameQueueItem {
        frameQueueItem() : subresource(nullptr) {}
        explicit frameQueueItem(std::shared_ptr<D3D11_MAPPED_SUBRESOURCE> inSubresource)
            : subresource(std::move(inSubresource)) {}

        std::shared_ptr<D3D11_MAPPED_SUBRESOURCE> subresource;
        int rowPitch;
    };

    struct exr_queue_item {
        exr_queue_item() : isEndOfStream(true) {}

        exr_queue_item(Microsoft::WRL::ComPtr<ID3D11Texture2D> cRGB, void* pRGBData,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> cDepth, void* pDepthData,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> cStencil, const D3D11_MAPPED_SUBRESOURCE mStencilData)
            : cRGB(std::move(cRGB)), pRGBData(pRGBData), cDepth(std::move(cDepth)), pDepthData(pDepthData),
              cStencil(std::move(cStencil)), mStencilData(mStencilData) {}

        bool isEndOfStream = false;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> cRGB{};
        void* pRGBData = nullptr;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> cDepth{};
        void* pDepthData = nullptr;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> cStencil{};
        D3D11_MAPPED_SUBRESOURCE mStencilData{.pData = nullptr, .RowPitch = 0, .DepthPitch = 0};
    };

    SafeQueue<frameQueueItem> videoFrameQueue;
    SafeQueue<exr_queue_item> exrImageQueue;

    // bool isEncodingThreadFinished = false;
    // std::condition_variable cvEncodingThreadFinished;
    // std::mutex mxEncodingThread;
    // std::thread thread_video_encoder;
    std::valarray<uint16_t> motionBlurAccBuffer;
    std::valarray<uint16_t> motionBlurTempBuffer;
    std::valarray<uint8_t> motionBlurDestBuffer;

    bool isEXREncodingThreadFinished = false;
    std::condition_variable cvEXREncodingThreadFinished;
    std::mutex mxEXREncodingThread;
    std::thread thread_exr_encoder;

    std::mutex mxFinish;
    std::mutex mxWriteFrame;

    int32_t width{};
    int32_t height{};
    int32_t open_exr_width;
    int32_t open_exr_height;
    int32_t framerate{};
    uint8_t motionBlurSamples{};
    int32_t audioBlockAlign{};
    int32_t inputAudioSampleRate{};
    int32_t inputAudioChannels{};
    int32_t outputAudioSampleRate{};
    int32_t outputAudioChannels{};
    std::string filename;
    std::string exrOutputPath;
    bool exportExr = false;
    uint64_t exrPTS = 0;
    float shutterPosition{};

    Session();

    ~Session();

    /*HRESULT createContext(const std::string& format, std::string filename, std::string exrOutputPath,
                          std::string fmtOptions, uint64_t width, uint64_t height, const std::string& inputPixelFmt,
                          uint32_t fps_num, uint32_t fps_den, uint8_t motionBlurSamples, float shutterPosition,
                          const std::string& outputPixelFmt, const std::string& vcodec, const std::string& voptions,
                          uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample,
                          std::string inputSampleFmt, uint32_t inputAlign, std::string outputSampleFmt,
                          std::string acodec, std::string aoptions);*/

    HRESULT createContext(const VKENCODERCONFIG& config, const std::wstring& in_filename, uint32_t in_width,
                          uint32_t in_height, const std::string& input_pixel_fmt, uint32_t fps_num, uint32_t fps_den,
                          uint32_t input_channels, uint32_t input_sample_rate, const std::string& input_sample_format,
                          uint32_t input_align, bool in_export_open_exr, uint32_t in_open_exr_width,
                          uint32_t in_open_exr_height);

    // HRESULT enqueueVideoFrame(BYTE* pData, int length);
    HRESULT enqueueVideoFrame(const D3D11_MAPPED_SUBRESOURCE& subresource);
    HRESULT enqueueEXRImage(const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& pDeviceContext,
                            const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cRGB,
                            const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cDepth);

    // void videoEncodingThread();
    // void exrEncodingThread();

    HRESULT writeVideoFrame(BYTE* pData, const int32_t length, const int rowPitch, const LONGLONG sampleTime);
    HRESULT writeAudioFrame(BYTE* pData, int32_t length, LONGLONG sampleTime);

    HRESULT finishVideo();
    HRESULT finishAudio();

    HRESULT endSession();
};
} // namespace Encoder