#pragma once

#include "SafeQueue.h"
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

extern "C" {
#pragma warning(push, 0)
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#pragma warning(pop)
}

// template<typename T>
// using std::shared_ptr = std::shared_ptr<T, std::function<void(T*)>>;

namespace Encoder {
class Session {
  public:
    AVOutputFormat* oformat = nullptr;
    AVFormatContext* fmtContext = nullptr;
    AVDictionary* fmtOptions = nullptr;

    AVCodec* videoCodec = nullptr;
    AVCodecContext* videoCodecContext = nullptr;
    // AVFrame* inputFrame = nullptr;
    // AVFrame* outputFrame = nullptr;
    AVStream* videoStream = nullptr;
    SwsContext* pSwsContext = nullptr;
    AVDictionary* videoOptions = nullptr;
    int64_t videoPTS = 0;
    int64_t motionBlurPTS = 0;

    AVCodec* audioCodec = nullptr;
    AVCodecContext* audioCodecContext = nullptr;
    AVFrame* inputAudioFrame = nullptr;
    AVFrame* outputAudioFrame = nullptr;
    AVStream* audioStream = nullptr;
    SwrContext* pSwrContext = nullptr;
    AVDictionary* audioOptions = nullptr;
    AVAudioFifo* audioSampleBuffer = nullptr;
    int64_t audioPTS = 0;

    bool isVideoFinished = false;
    bool isAudioFinished = false;
    bool isSessionFinished = false;
    bool isCapturing = false;
    bool isBeingDeleted = false;

    std::mutex mxFormatContext;
    std::condition_variable cvFormatContext;

    std::mutex mxEndSession;
    std::condition_variable cvEndSession;

    struct frameQueueItem {
        frameQueueItem() : data(nullptr) {}
        explicit frameQueueItem(std::shared_ptr<std::valarray<uint8_t>> bytes) : data(std::move(bytes)) {}

        std::shared_ptr<std::valarray<uint8_t>> data;
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
        // void* pStencilData;
    };

    SafeQueue<frameQueueItem> videoFrameQueue;
    SafeQueue<exr_queue_item> exrImageQueue;

    bool isVideoContextCreated = false;
    bool isAudioContextCreated = false;
    bool isFormatContextCreated = false;
    bool isEncodingThreadFinished = false;
    std::condition_variable cvEncodingThreadFinished;
    std::mutex mxEncodingThread;
    std::thread thread_video_encoder;
    std::valarray<uint16_t> motionBlurAccBuffer;
    std::valarray<uint16_t> motionBlurTempBuffer;
    std::valarray<uint8_t> motionBlurDestBuffer;

    bool isEXREncodingThreadFinished = false;
    std::condition_variable cvEXREncodingThreadFinished;
    std::mutex mxEXREncodingThread;
    std::thread thread_exr_encoder;

    // std::condition_variable cvFormatContext;

    std::mutex mxFinish;
    std::mutex mxWriteFrame;

    int32_t width{};
    int32_t height{};
    int32_t framerate{};
    // float audioSampleRateMultiplier;
    uint8_t motionBlurSamples{};
    int32_t audioBlockAlign{};
    AVPixelFormat outputPixelFormat;
    AVPixelFormat inputPixelFormat;
    AVSampleFormat inputAudioSampleFormat;
    AVSampleFormat outputAudioSampleFormat;
    int32_t inputAudioSampleRate{};
    int32_t inputAudioChannels{};
    int32_t outputAudioSampleRate{};
    int32_t outputAudioChannels{};
    std::string filename;
    std::string exrOutputPath;
    uint64_t exrPTS = 0;
    float shutterPosition{};
    // LPWSTR *outputDir;
    // LPWSTR *outputFile;
    // FILE *file;

    Session();

    ~Session();

    HRESULT createContext(const std::string& format, std::string filename, std::string exrOutputPath,
                          std::string fmtOptions, uint64_t width, uint64_t height, const std::string& inputPixelFmt,
                          uint32_t fps_num, uint32_t fps_den, uint8_t motionBlurSamples, float shutterPosition,
                          const std::string& outputPixelFmt, const std::string& vcodec, const std::string& voptions,
                          uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample,
                          std::string inputSampleFmt, uint32_t inputAlign, std::string outputSampleFmt,
                          std::string acodec, std::string aoptions);

    HRESULT enqueueVideoFrame(BYTE* pData, int length);
    HRESULT enqueueEXRImage(const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& pDeviceContext,
                            const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cRGB,
                            const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cDepth,
                            const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cStencil);

    void videoEncodingThread();
    void exrEncodingThread();

    HRESULT writeVideoFrame(const BYTE* pData, int32_t length, LONGLONG sampleTime);
    HRESULT writeAudioFrame(const BYTE* pData, int32_t length, LONGLONG sampleTime);

    HRESULT finishVideo();
    HRESULT finishAudio();

    HRESULT endSession();

  private:
    HRESULT createVideoContext(int32_t input_width, int32_t input_height, const std::string& inputPixelFormatString,
                               int32_t fps_num, int32_t fps_den, uint8_t input_motionBlurSamples,
                               float input_shutterPosition, const std::string& outputPixelFormatString,
                               const std::string& vcodec, const std::string& preset);
    HRESULT createAudioContext(int32_t inputChannels, int32_t inputSampleRate, int32_t inputBitsPerSample,
                               const std::string& inputSampleFormat, int32_t inputAlignment,
                               const std::string& outputSampleFormatString, const std::string& acodec,
                               const std::string& preset);
    HRESULT createFormatContext(const std::string& format, const std::string& filename, std::string exrOutputPath,
                                const std::string& fmtOptions);
    HRESULT createVideoFrames(int32_t srcWidth, int32_t srcHeight, AVPixelFormat srcFmt, int32_t dstWidth,
                              int32_t dstHeight, AVPixelFormat dstFmt);
    HRESULT createAudioFrames(int32_t inputChannels, AVSampleFormat inputSampleFmt, int32_t inputSampleRate,
                              int32_t outputChannels, AVSampleFormat outputSampleFmt, int32_t outputSampleRate);
};
} // namespace Encoder