#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 26812)
#include "encoder.h"
#include "logger.h"
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfRgba.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <xutility>
#pragma warning(pop)

// #pragma warning(push, 0)
//  extern "C" {
// #include <libavutil/imgutils.h>
// #include <libavutil/pixdesc.h>
// }
// #pragma warning(pop)

//#define IGNORE_DATA_LOSS_WARNING(o)                                                                                    \
//    {                                                                                                                  \
//        __pragma(warning(push)) __pragma(warning(disable : 4244))(o);                                                  \
//        __pragma(warning(pop))                                                                                         \
//    }                                                                                                                  \
//    REQUIRE_SEMICOLON

namespace Encoder {

// const AVRational MF_TIME_BASE = {1, 10000000};

Session::Session() : videoFrameQueue(128), exrImageQueue(16) {
    PRE();
    LOG(LL_NFO, "Opening session: ", reinterpret_cast<uint64_t>(this));
    POST();
}

Session::~Session() {
    PRE();
    LOG(LL_NFO, "Closing session: ", reinterpret_cast<uint64_t>(this));
    this->isCapturing = false;
    //LOG_CALL(LL_DBG, this->videoFrameQueue.enqueue(Encoder::Session::frameQueueItem(nullptr, 0)));

    //if (thread_video_encoder.joinable()) {
    //    thread_video_encoder.join();
    //}

    //LOG_CALL(LL_DBG, this->exrImageQueue.enqueue(Encoder::Session::exr_queue_item()));

    //if (thread_exr_encoder.joinable()) {
    //    thread_exr_encoder.join();
    //}

    LOG_CALL(LL_DBG, this->finishVideo());
    LOG_CALL(LL_DBG, this->finishAudio());
    LOG_CALL(LL_DBG, this->endSession());
    //LOG_CALL(LL_DBG, pVoukoder->Close(true));
    this->isBeingDeleted = true;

    // LOG_CALL(LL_DBG, av_free(this->oformat));
    // LOG_CALL(LL_DBG, av_free(this->fmtContext));
    // LOG_CALL(LL_DBG, avcodec_close(this->videoCodecContext));
    // LOG_CALL(LL_DBG, av_free(this->videoCodecContext));
    // LOG_CALL(LL_DBG, av_free(this->inputFrame));
    // LOG_CALL(LL_DBG, av_free(this->outputFrame));
    // LOG_CALL(LL_DBG, sws_freeContext(this->pSwsContext));
    // LOG_CALL(LL_DBG, swr_free(&this->pSwrContext));
    // if (this->videoOptions) {
    //    LOG_CALL(LL_DBG, av_dict_free(&this->videoOptions));
    //}
    // if (this->audioOptions) {
    //    LOG_CALL(LL_DBG, av_dict_free(&this->audioOptions));
    //}
    // if (this->fmtOptions) {
    //    LOG_CALL(LL_DBG, av_dict_free(&this->fmtOptions));
    //}
    // LOG_CALL(LL_DBG, avcodec_close(this->audioCodecContext));
    // LOG_CALL(LL_DBG, av_free(this->audioCodecContext));
    // LOG_CALL(LL_DBG, av_free(this->inputAudioFrame));
    // LOG_CALL(LL_DBG, av_free(this->outputAudioFrame));
    // LOG_CALL(LL_DBG, av_audio_fifo_free(this->audioSampleBuffer));
    POST();
}

// HRESULT Session::createContext(const std::string& format, std::string filename, std::string exrOutputPath,
//                               std::string fmtOptions, uint64_t width, uint64_t height,
//                               const std::string& inputPixelFmt, uint32_t fps_num, uint32_t fps_den,
//                               uint8_t motionBlurSamples, float shutterPosition, const std::string& outputPixelFmt,
//                               const std::string& vcodec_str, const std::string& voptions, uint32_t inputChannels,
//                               uint32_t inputSampleRate, uint32_t inputBitsPerSample, std::string inputSampleFmt,
//                               uint32_t inputAlign, std::string outputSampleFmt, std::string acodec_str,
//                               std::string aoptions) {

HRESULT Session::createContext(const VKENCODERCONFIG& config, const std::wstring& filename, uint32_t width,
                               uint32_t height, const std::string& inputPixelFmt, uint32_t fps_num, uint32_t fps_den,
                               uint32_t inputChannels, uint32_t inputSampleRate, const std::string& inputSampleFormat,
                               uint32_t inputAlign) {
    PRE();
    // this->oformat = av_guess_format(format.c_str(), nullptr, nullptr);
    // RET_IF_NULL(this->oformat, "Could not find format: " + format, E_FAIL);

    // REQUIRE(this->createVideoContext(width, height, inputPixelFmt, fps_num, fps_den, motionBlurSamples,
    // shutterPosition,
    //                                 outputPixelFmt, vcodec_str, voptions),
    //        "Failed to create video codec context.");
    // REQUIRE(this->createAudioContext(inputChannels, inputSampleRate, inputBitsPerSample, inputSampleFmt, inputAlign,
    //                                 outputSampleFmt, acodec_str, aoptions),
    //        "Failed to create audio codec context.");
    // REQUIRE(this->createFormatContext(format, filename, exrOutputPath, fmtOptions), "Failed to create format
    // context.");

    ASSERT_RUNTIME(filename.length() < sizeof(VKENCODERINFO::filename) / sizeof(wchar_t), "File name too long.");
    ASSERT_RUNTIME(inputChannels == 1 || inputChannels == 2 || inputChannels == 6,
                   "Invalid number of audio channels, only 1, 2 and 6 (5.1) are supported.");

    Microsoft::WRL::ComPtr<IVoukoder> m_pVoukoder = nullptr;

    REQUIRE(CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder,
                             (void**)m_pVoukoder.GetAddressOf()),
            "Failed to create an instance of IVoukoder interface.");

    REQUIRE(m_pVoukoder->SetConfig(config), "Failed to set Voukoder config");

    VKENCODERINFO vkInfo{
        .application = L"Extended Video Export",
        .video{
            .enabled = true,
            .width = static_cast<int>(width),
            .height = static_cast<int>(height),
            .timebase = {static_cast<int>(fps_den), static_cast<int>(fps_num)},
            .aspectratio{1, 1},
            .fieldorder = FieldOrder::Progressive, // TODO
        },
        .audio{
            .enabled = true,
            .samplerate = static_cast<int>(inputSampleRate),
            .channellayout =
                [inputChannels] {
                    switch (inputChannels) {
                    case 1:
                        return ChannelLayout::Mono;
                    case 2:
                        return ChannelLayout::Stereo;
                    default:
                        return ChannelLayout::FivePointOne;
                    }
                }(),
            .numberChannels = static_cast<int>(inputChannels),
        },
    };
    filename.copy(vkInfo.filename, std::size(vkInfo.filename));
    // std::copy(filename.begin(), filename.end(), vkInfo.filename);

    REQUIRE(m_pVoukoder->Open(vkInfo), "Failed to open Voukoder context.");

    videoFrame = {
        .buffer = new byte*[1], // We'll set this in writeVideoFrame() function
        .rowsize = new int[1],  // We'll set this in writeVideoFrame() function
        .planes = 1,
        .width = static_cast<int>(width),
        .height = static_cast<int>(height),
        .pass = 1,
    };
    // std::copy(inputPixelFmt.begin(), inputPixelFmt.end(), &videoFrame.format);
    inputPixelFmt.copy(videoFrame.format, std::size(videoFrame.format));

    audioChunk = {
        .buffer = new byte*[1], // We'll set this in writeAudioFrame() function
        .samples = 0,           // We'll set this in writeAudioFrame() function
        .blockSize = static_cast<int>(inputAlign),
        .planes = 1,
        .sampleRate = static_cast<int>(inputSampleRate),
        .layout = vkInfo.audio.channellayout,
    };
    inputSampleFormat.copy(audioChunk.format, std::size(audioChunk.format));
    // std::copy(inputSampleFormat.begin(), inputSampleFormat.end(), &audioChunk.format);

    pVoukoder = std::move(m_pVoukoder);

    this->thread_video_encoder = std::thread(&Session::videoEncodingThread, this);
    this->thread_exr_encoder = std::thread(&Session::exrEncodingThread, this);

    this->isCapturing = true;

    POST();
    return S_OK;
}

// HRESULT Session::createVideoContext(const int32_t input_width, const int32_t input_height,
//                                    const std::string& inputPixelFormatString, const int32_t fps_num,
//                                    const int32_t fps_den, const uint8_t input_motionBlurSamples,
//                                    const float input_shutterPosition, const std::string& outputPixelFormatString,
//                                    const std::string& vcodec, const std::string& preset) {
//    PRE();
//    if (this->isBeingDeleted) {
//        POST();
//        return E_FAIL;
//    }
//
//    if (vcodec.empty()) {
//        this->videoCodecContext = nullptr;
//        this->isVideoContextCreated = true;
//        POST();
//        return S_OK;
//    }
//
//    LOG(LL_NFO, "Creating video context:");
//    LOG(LL_NFO, "  encoder: ", vcodec);
//    LOG(LL_NFO, "  options: ", preset);
//
//    this->inputPixelFormat = av_get_pix_fmt(inputPixelFormatString.c_str());
//    if (this->inputPixelFormat == AV_PIX_FMT_NONE) {
//        LOG(LL_ERR, "Unknown input pixel format specified: ", inputPixelFormatString);
//        POST();
//        return E_FAIL;
//    }
//
//    this->outputPixelFormat = av_get_pix_fmt(outputPixelFormatString.c_str());
//    if (this->outputPixelFormat == AV_PIX_FMT_NONE) {
//        LOG(LL_ERR, "Unknown output pixel format specified: ", outputPixelFormatString);
//        POST();
//        return E_FAIL;
//    }
//    const auto numPixels = static_cast<size_t>(input_width) * static_cast<size_t>(input_height);
//    this->width = input_width;
//    this->height = input_height;
//    this->motionBlurSamples = input_motionBlurSamples;
//    this->motionBlurAccBuffer = std::valarray<uint16_t>(numPixels * 4);
//    this->motionBlurTempBuffer = std::valarray<uint16_t>(numPixels * 4);
//    this->motionBlurDestBuffer = std::valarray<uint8_t>(numPixels * 4);
//    this->shutterPosition = input_shutterPosition;
//
//    // this->audioSampleRateMultiplier = ((float)fps_num * ((float)motionBlurSamples + 1)) / ((float)fps_den
//    // * 60.0f);
//
//    this->videoCodec = avcodec_find_encoder_by_name(vcodec.c_str());
//    RET_IF_NULL(this->videoCodec, "Could not find video codec:" + vcodec, E_FAIL);
//
//    this->videoCodecContext = avcodec_alloc_context3(this->videoCodec);
//    RET_IF_NULL(this->videoCodecContext, "Could not allocate context for the video codec", E_FAIL);
//
//    av_dict_parse_string(&this->videoOptions, preset.c_str(), "=", "/", 0);
//    // av_set_options_string(this->videoCodecContext, preset.c_str(), "=", "/");
//
//    RET_IF_FAILED(this->createVideoFrames(input_width, input_height, this->inputPixelFormat, input_width,
//    input_height,
//                                          this->outputPixelFormat),
//                  "Could not create video frames", E_FAIL);
//
//    this->videoCodecContext->codec_id = this->videoCodec->id;
//    this->videoCodecContext->pix_fmt = this->outputPixelFormat;
//    this->videoCodecContext->width = input_width;
//    this->videoCodecContext->height = input_height;
//    this->videoCodecContext->time_base = av_make_q(fps_den, fps_num);
//    this->videoCodecContext->framerate = av_make_q(fps_num, fps_den);
//    this->videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
//
//    if (this->oformat->flags & AVFMT_GLOBALHEADER) {
//        this->videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//    }
//
//    RET_IF_FAILED_AV(avcodec_open2(this->videoCodecContext, this->videoCodec, &this->videoOptions),
//                     "Could not open video codec", E_FAIL);
//
//    this->thread_video_encoder = std::thread(&Session::videoEncodingThread, this);
//    this->thread_exr_encoder = std::thread(&Session::exrEncodingThread, this);
//
//    LOG(LL_NFO, "Video context was created successfully.");
//    this->isVideoContextCreated = true;
//    POST();
//    return S_OK;
//}
//
// HRESULT Session::createAudioContext(const int32_t inputChannels, const int32_t inputSampleRate,
//                                    const int32_t inputBitsPerSample, const std::string& inputSampleFormat,
//                                    const int32_t inputAlignment, const std::string& outputSampleFormatString,
//                                    const std::string& acodec, const std::string& preset) {
//    PRE();
//    if (this->isBeingDeleted) {
//        POST();
//        return E_FAIL;
//    }
//
//    if (acodec.empty()) {
//        this->audioCodecContext = nullptr;
//        this->isAudioContextCreated = true;
//        POST();
//        return S_OK;
//    }
//
//    LOG(LL_NFO, "Creating audio context:");
//    LOG(LL_NFO, "  encoder: ", acodec);
//    LOG(LL_NFO, "  options: ", preset);
//    // AVCodecID audioCodecId = AV_CODEC_ID_PCM_S16LE;
//
//    // this->inputAudioSampleRate = static_cast<uint32_t>(inputSampleRate * this->audioSampleRateMultiplier);
//    this->inputAudioSampleRate = inputSampleRate;
//    this->inputAudioChannels = inputChannels;
//    this->outputAudioChannels = inputChannels; // FIXME
//
//    this->inputAudioSampleFormat = av_get_sample_fmt(inputSampleFormat.c_str());
//    if (this->inputAudioSampleFormat == AV_SAMPLE_FMT_NONE) {
//        LOG(LL_ERR, "Unknown audio sample format specified: ", inputSampleFormat);
//        POST();
//        return E_FAIL;
//    }
//
//    this->outputAudioSampleFormat = av_get_sample_fmt(outputSampleFormatString.c_str());
//    if (this->outputAudioSampleFormat == AV_SAMPLE_FMT_NONE) {
//        LOG(LL_ERR, "Unknown audio sample format specified: ", outputSampleFormatString);
//        POST();
//        return E_FAIL;
//    }
//
//    this->audioCodec = avcodec_find_encoder_by_name(acodec.c_str());
//    RET_IF_NULL(this->audioCodec, "Could not find audio codec:" + acodec, E_FAIL);
//    this->audioCodecContext = avcodec_alloc_context3(this->audioCodec);
//    RET_IF_NULL(this->audioCodecContext, "Could not allocate context for the audio codec", E_FAIL);
//
//    // av_set_options_string(this->audioCodecContext, preset.c_str(), "=", "/");
//
//    this->audioCodecContext->codec_id = this->audioCodec->id;
//    this->audioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
//    this->audioCodecContext->channels = inputChannels;
//    this->audioCodecContext->sample_fmt = this->outputAudioSampleFormat;
//    this->audioCodecContext->time_base = {1, inputSampleRate};
//    // this->audioCodecContext->frame_size = 256;
//    this->audioCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
//
//    if (this->oformat->flags & AVFMT_GLOBALHEADER) {
//        this->audioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//    }
//    av_dict_parse_string(&this->audioOptions, preset.c_str(), "=", "/", 0);
//    this->audioBlockAlign = inputAlignment;
//
//    RET_IF_FAILED_AV(avcodec_open2(this->audioCodecContext, this->audioCodec, &this->audioOptions),
//                     "Could not open audio codec", E_FAIL);
//
//    this->outputAudioSampleRate = this->audioCodecContext->sample_rate;
//    LOG(LL_TRC, this->outputAudioSampleRate);
//    RET_IF_FAILED(this->createAudioFrames(inputChannels, this->inputAudioSampleFormat, this->inputAudioSampleRate,
//                                          inputChannels, this->outputAudioSampleFormat, this->outputAudioSampleRate),
//                  "Could not create audio frames", E_FAIL);
//
//    LOG(LL_NFO, "Audio context was created successfully.");
//    this->isAudioContextCreated = true;
//
//    POST();
//    return S_OK;
//}

// HRESULT Session::createFormatContext(const std::string& format, const std::string& filename, std::string
// exrOutputPath,
//                                     const std::string& fmtOptions) {
//    PRE();
//    if (this->isBeingDeleted) {
//        POST();
//        return E_FAIL;
//    }
//
//    std::lock_guard<std::mutex> guard(this->mxFormatContext);
//
//    this->exrOutputPath = std::move(exrOutputPath);
//
//    this->filename = filename;
//    LOG(LL_NFO, "Exporting to file: ", this->filename);
//
//    /*this->oformat = av_guess_format(format.c_str(), NULL, NULL);
//    RET_IF_NULL(this->oformat, "Could not create format", E_FAIL);*/
//    RET_IF_FAILED_AV(avformat_alloc_output_context2(&this->fmtContext, this->oformat, NULL, NULL),
//                     "Could not allocate format context", E_FAIL);
//    RET_IF_NULL(this->fmtContext, "Could not allocate format context", E_FAIL);
//    // this->fmtContext = avformat_alloc_context();
//
//    this->fmtContext->oformat = this->oformat;
//    /*if (this->videoCodec) {
//            this->fmtContext->video_codec_id = this->videoCodec->id;
//    }
//    if (this->audioCodec) {
//            this->fmtContext->audio_codec_id = this->audioCodec->id;
//    }*/
//
//    // this->fmtContext->oformat = this->oformat;
//
//    /*if (this->fmtContext->oformat->flags & AVFMT_GLOBALHEADER)
//    {
//            if (this->videoCodecContext) {
//                    this->videoCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
//            }
//            if (this->audioCodecContext) {
//                    this->audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
//            }
//    }*/
//
//    // av_set_options_string(this->fmtContext, fmtPreset.c_str(), "=", "/");
//    av_dict_parse_string(&this->fmtOptions, fmtOptions.c_str(), "=", "/", 0);
//
//    // av_opt_set(this->fmtContext->priv_data, "path", filename, 0);
//    strcpy_s(this->fmtContext->filename, this->filename.c_str());
//    // av_opt_set(this->fmtContext, "filename", this->filename.c_str(), 0);
//
//    bool hasVideo = false;
//    bool hasAudio = false;
//
//    if (this->videoCodecContext &&
//        (!this->oformat->query_codec || this->oformat->query_codec(this->videoCodec->id, 0) == 1)) {
//        this->videoStream = avformat_new_stream(this->fmtContext, this->videoCodec);
//        RET_IF_NULL(this->videoStream, "Could not create video stream", E_FAIL);
//        avcodec_parameters_from_context(this->videoStream->codecpar, this->videoCodecContext);
//        // avcodec_copy_context(this->videoStream->codec, this->videoCodecContext);
//        this->videoStream->time_base = this->videoCodecContext->time_base;
//        // this->videoStream->index = this->fmtContext->nb_streams - 1;
//        hasVideo = true;
//    }
//
//    if (this->audioCodecContext &&
//        (!this->oformat->query_codec || this->oformat->query_codec(this->audioCodec->id, 0) == 1)) {
//        this->audioStream = avformat_new_stream(this->fmtContext, this->audioCodec);
//        RET_IF_NULL(this->audioStream, "Could not create audio stream", E_FAIL);
//        avcodec_parameters_from_context(this->audioStream->codecpar, this->audioCodecContext);
//        // avcodec_copy_context(this->audioStream->codec, this->audioCodecContext);
//        this->audioStream->time_base = this->audioCodecContext->time_base;
//        // this->audioStream->index = this->fmtContext->nb_streams - 1;
//        hasAudio = true;
//    }
//
//    if (!hasAudio && !hasVideo) {
//        LOG(LL_NFO, "Both audio and video are disabled. Cannot create format context");
//        this->isCapturing = true;
//        this->isFormatContextCreated = true;
//        this->cvFormatContext.notify_all();
//        POST();
//        return E_FAIL;
//    }
//
//    RET_IF_FAILED_AV(avio_open(&this->fmtContext->pb, filename.c_str(), AVIO_FLAG_WRITE), "Could not open output
//    file",
//                     E_FAIL);
//    RET_IF_NULL(this->fmtContext->pb, "Could not open output file", E_FAIL);
//    RET_IF_FAILED_AV(avformat_write_header(this->fmtContext, &this->fmtOptions), "Could not write header", E_FAIL);
//    LOG(LL_NFO, "Format context was created successfully.");
//    this->isCapturing = true;
//    this->isFormatContextCreated = true;
//    this->cvFormatContext.notify_all();
//
//    POST();
//    return S_OK;
//}

HRESULT Session::enqueueEXRImage(const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& pDeviceContext,
                                 const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cRGB,
                                 const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cDepth,
                                 const Microsoft::WRL::ComPtr<ID3D11Texture2D>& cStencil) {
    PRE();

    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

    D3D11_MAPPED_SUBRESOURCE mHDR = {nullptr};
    D3D11_MAPPED_SUBRESOURCE mDepth = {nullptr};
    D3D11_MAPPED_SUBRESOURCE mStencil = {nullptr};

    if (cRGB) {
        REQUIRE(pDeviceContext->Map(cRGB.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &mHDR), "Failed to map HDR texture");
    }

    if (cDepth) {
        REQUIRE(pDeviceContext->Map(cDepth.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &mDepth),
                "Failed to map depth texture");
    }

    if (cStencil) {
        REQUIRE(pDeviceContext->Map(cStencil.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &mStencil),
                "Failed to map stencil texture");
    }

    this->exrImageQueue.enqueue(exr_queue_item(cRGB, mHDR.pData, cDepth, mDepth.pData, cStencil, mStencil));

    POST();
    return S_OK;
}

HRESULT Session::enqueueVideoFrame(BYTE* pData, int rowPitch, int size) {
    PRE();

    // if (!this->videoCodecContext) {
    //    POST();
    //    return S_OK;
    //}

    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }
    const auto pVector = std::make_shared<std::valarray<uint8_t>>(size);

    std::copy_n(pData, size, std::begin(*pVector));

    const frameQueueItem item(pVector, rowPitch);
    this->videoFrameQueue.enqueue(item);
    POST();
    return S_OK;
}

void Session::videoEncodingThread() {
    PRE();
    std::lock_guard lock(this->mxEncodingThread);
    try {
        int16_t k = 0;
        bool firstFrame = true;
        frameQueueItem item = this->videoFrameQueue.dequeue();
        while (item.data != nullptr) {
            auto& data = *(item.data);
            if (this->motionBlurSamples == 0) {
                LOG(LL_NFO, "Encoding frame: ", this->videoPTS);
                REQUIRE(this->writeVideoFrame(std::begin(data), static_cast<int32_t>(item.data->size()), item.rowPitch,
                                              this->videoPTS++),
                        "Failed to write video frame.");
            } else {
                const auto frameRemainder =
                    static_cast<uint8_t>(this->motionBlurPTS++ % (static_cast<uint64_t>(this->motionBlurSamples) + 1));
                const float currentShutterPosition =
                    static_cast<float>(frameRemainder) / static_cast<float>(this->motionBlurSamples + 1);
                std::ranges::copy(data, std::begin(this->motionBlurTempBuffer));
                if (frameRemainder == this->motionBlurSamples) {
                    // Flush motion blur buffer
                    this->motionBlurAccBuffer += this->motionBlurTempBuffer;
                    this->motionBlurAccBuffer /= ++k;
                    std::ranges::copy(this->motionBlurAccBuffer, std::begin(this->motionBlurDestBuffer));
                    REQUIRE(this->writeVideoFrame(std::begin(this->motionBlurDestBuffer),
                                                  this->motionBlurDestBuffer.size(), item.rowPitch, this->videoPTS++),
                            "Failed to write video frame.");
                    k = 0;
                    firstFrame = true;
                } else if (currentShutterPosition >= this->shutterPosition) {
                    if (firstFrame) {
                        // Reset accumulation buffer
                        this->motionBlurAccBuffer = this->motionBlurTempBuffer;
                        firstFrame = false;
                    } else {
                        this->motionBlurAccBuffer += this->motionBlurTempBuffer;
                    }
                    k++;
                }
            }
            item = this->videoFrameQueue.dequeue();
        }
    } catch (...) {
        // Do nothing
    }
    this->isEncodingThreadFinished = true;
    this->cvEncodingThreadFinished.notify_all();
    POST();
}

void Session::exrEncodingThread() {
    PRE();
    std::lock_guard<std::mutex> lock(this->mxEXREncodingThread);
    Imf::setGlobalThreadCount(8);
    try {
        exr_queue_item item = this->exrImageQueue.dequeue();
        while (!item.isEndOfStream) {
            struct RGBA {
                half R;
                half G;
                half B;
                half A;
            };

            struct Depth {
                float depth;
            };

            Imf::Header header(this->width, this->height);
            Imf::FrameBuffer framebuffer;

            if (item.cRGB != nullptr) {
                LOG_CALL(LL_DBG, header.channels().insert("R", Imf::Channel(Imf::HALF)));
                LOG_CALL(LL_DBG, header.channels().insert("G", Imf::Channel(Imf::HALF)));
                LOG_CALL(LL_DBG, header.channels().insert("B", Imf::Channel(Imf::HALF)));
                LOG_CALL(LL_DBG, header.channels().insert("SSS", Imf::Channel(Imf::HALF)));
                const auto mHDRArray = static_cast<RGBA*>(item.pRGBData);

                LOG_CALL(LL_DBG, framebuffer.insert("R", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].R),
                                                                    sizeof(RGBA), sizeof(RGBA) * this->width)));

                LOG_CALL(LL_DBG, framebuffer.insert("G", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].G),
                                                                    sizeof(RGBA), sizeof(RGBA) * this->width)));

                LOG_CALL(LL_DBG, framebuffer.insert("B", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].B),
                                                                    sizeof(RGBA), sizeof(RGBA) * this->width)));

                LOG_CALL(LL_DBG,
                         framebuffer.insert("SSS", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].A),
                                                              sizeof(RGBA), sizeof(RGBA) * this->width)));
            }

            if (item.cDepth != nullptr) {
                LOG_CALL(LL_DBG, header.channels().insert("depth.Z", Imf::Channel(Imf::FLOAT)));
                // header.channels().insert("objectID", Imf::Channel(Imf::UINT));
                const auto mDSArray = static_cast<Depth*>(item.pDepthData);

                LOG_CALL(LL_DBG, framebuffer.insert("depth.Z",
                                                    Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&mDSArray[0].depth),
                                                               sizeof(Depth), sizeof(Depth) * this->width)));
            }

            std::vector<uint32_t> stencilBuffer;
            if (item.cStencil != nullptr) {
                stencilBuffer = std::vector<uint32_t>(static_cast<size_t>(item.mStencilData.RowPitch) *
                                                      static_cast<size_t>(this->height));
                auto mSArray = static_cast<uint8_t*>(item.mStencilData.pData);

                for (uint32_t i = 0; i < item.mStencilData.RowPitch * this->height; i++) {
                    stencilBuffer[i] = static_cast<uint32_t>(mSArray[i]);
                }

                LOG_CALL(LL_DBG, header.channels().insert("objectID", Imf::Channel(Imf::UINT)));

                LOG_CALL(LL_DBG,
                         framebuffer.insert("objectID",
                                            Imf::Slice(Imf::UINT, reinterpret_cast<char*>(stencilBuffer.data()),
                                                       sizeof(uint32_t),
                                                       static_cast<size_t>(item.mStencilData.RowPitch) * size_t{4})));
            }

            if (CreateDirectoryA(this->exrOutputPath.c_str(), nullptr) || ERROR_ALREADY_EXISTS == GetLastError()) {

                std::stringstream sstream;
                sstream << std::setw(5) << std::setfill('0') << this->exrPTS++;
                Imf::OutputFile file((this->exrOutputPath + "\\frame" + sstream.str() + ".exr").c_str(), header);
                LOG_CALL(LL_DBG, file.setFrameBuffer(framebuffer));
                LOG_CALL(LL_DBG, file.writePixels(this->height));
            }

            item = this->exrImageQueue.dequeue();
        }
    } catch (std::exception& ex) {
        LOG(LL_ERR, ex.what());
    }
    this->isEXREncodingThreadFinished = true;
    this->cvEXREncodingThreadFinished.notify_all();
    POST();
}

HRESULT Session::writeVideoFrame(BYTE* pData, const int32_t length, const int rowPitch, const LONGLONG sampleTime) {
    PRE();
    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

    // Wait until format context is created
    //if (pVoukoder == nullptr) {
    //    std::unique_lock<std::mutex> lk(this->mxVoukoderContext);
    //    while (pVoukoder == nullptr) {
    //        this->cvVoukoderContext.wait_for(lk, std::chrono::milliseconds(5));
    //    }
    //}

    videoFrame.buffer[0] = pData;
    videoFrame.rowsize[0] = rowPitch;

    REQUIRE(pVoukoder->SendVideoFrame(videoFrame), "Failed to send video frame to Voukoder.");

    POST();
    return S_OK;
}

HRESULT Session::writeAudioFrame(BYTE* pData, const int32_t length, LONGLONG sampleTime) {
    PRE();

    /*if (!this->audioCodecContext) {
        POST();
        return S_OK;
    }*/

    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

    // Wait until format context is created
    //if (pVoukoder == nullptr) {
    //    std::unique_lock<std::mutex> lk(this->mxVoukoderContext);
    //    while (pVoukoder == nullptr) {
    //        this->cvVoukoderContext.wait_for(lk, std::chrono::milliseconds(5));
    //    }
    //}

    audioChunk.buffer[0] = pData;
    audioChunk.samples = length;

    REQUIRE(pVoukoder->SendAudioSampleChunk(audioChunk), "Failed to send audio chunk to Voukoder.");

    POST();
    return S_OK;
}

HRESULT Session::finishVideo() {
    PRE();
    std::lock_guard<std::mutex> guard(this->mxFinish);

    if (!this->isVideoFinished) {
        // Wait until the video encoding thread is finished.
        if (thread_video_encoder.joinable()) {
            // Write end of the stream object with a nullptr
            this->videoFrameQueue.enqueue(frameQueueItem(nullptr, 0));
            std::unique_lock<std::mutex> lock(this->mxEncodingThread);
            while (!this->isEncodingThreadFinished) {
                this->cvEncodingThreadFinished.wait(lock);
            }

            thread_video_encoder.join();
        }

        // Wait until the exr encoding thread is finished
        if (thread_exr_encoder.joinable()) {
            // Write end of the stream object with a nullptr
            this->exrImageQueue.enqueue(exr_queue_item());
            std::unique_lock<std::mutex> lock(this->mxEXREncodingThread);
            while (!this->isEXREncodingThreadFinished) {
                this->cvEXREncodingThreadFinished.wait(lock);
            }

            thread_exr_encoder.join();
        }

        // Write delayed frames
        //{
        //    AVPacket pkt;

        //    av_init_packet(&pkt);
        //    pkt.data = nullptr;
        //    pkt.size = 0;

        //    // int got_packet;
        //    // avcodec_encode_video2(this->videoCodecContext, &pkt, NULL, &got_packet);
        //    avcodec_send_frame(this->videoCodecContext, nullptr);
        //    while (SUCCEEDED(avcodec_receive_packet(this->videoCodecContext, &pkt))) {
        //        std::lock_guard<std::mutex> guard(this->mxWriteFrame);
        //        av_packet_rescale_ts(&pkt, this->videoCodecContext->time_base, this->videoStream->time_base);
        //        // pkt.dts = outputFrame->pts;
        //        pkt.stream_index = this->videoStream->index;
        //        av_interleaved_write_frame(this->fmtContext, &pkt);
        //        // avcodec_encode_video2(this->videoCodecContext, &pkt, NULL, &got_packet);
        //    }

        //    av_packet_unref(&pkt);
        //}

        delete videoFrame.buffer;
        videoFrame.buffer = nullptr;
        delete videoFrame.rowsize;
        videoFrame.rowsize = nullptr;

        this->isVideoFinished = true;
    }

    POST();
    return S_OK;
}

HRESULT Session::finishAudio() {
    PRE();
    std::lock_guard<std::mutex> guard(this->mxFinish);
    /*if (!(this->isAudioFinished || this->isBeingDeleted)) {
        this->isAudioFinished = true;
        POST();
        return S_OK;
    }*/

    // Write delayed frames
    //{

    //    LOG(LL_WRN, "FIXME: Audio samples discarded:", av_audio_fifo_size(this->audioSampleBuffer));

    //    // AVPacket pkt;

    //    // av_init_packet(&pkt);
    //    // pkt.data = NULL;
    //    // pkt.size = 0;

    //    // int got_packet;
    //    // avcodec_encode_audio2(this->audioCodecContext, &pkt, NULL, &got_packet);
    //    // while (got_packet != 0) {
    //    //	std::lock_guard<std::mutex> guard(this->mxWriteFrame);
    //    //	//av_packet_rescale_ts(&pkt, this->audioCodecContext->time_base, this->audioStream->time_base);
    //    //	pkt.stream_index = this->audioStream->index;
    //    //	av_interleaved_write_frame(this->fmtContext, &pkt);
    //    //	avcodec_encode_audio2(this->audioCodecContext, &pkt, NULL, &got_packet);
    //    //}

    //    // av_free_packet(&pkt);
    //    // av_packet_unref(&pkt);
    //}
    if (!this->isAudioFinished) {
        delete this->audioChunk.buffer;
        this->audioChunk.buffer = nullptr;
    }

    this->isAudioFinished = true;
    POST();
    return S_OK;
}

HRESULT Session::endSession() {
    PRE();
    std::lock_guard<std::mutex> lock(this->mxEndSession);

    if (this->isSessionFinished || this->isBeingDeleted) {
        POST();
        return S_OK;
    }

    // this->videoFrameQueue.enqueue(Encoder::Session::frameQueueItem(nullptr, -1));

    while (true) {
        std::lock_guard<std::mutex> guard(this->mxFinish);
        if (this->isVideoFinished && this->isAudioFinished) {
            break;
        }
    }

    this->isCapturing = false;
    LOG(LL_NFO, "Ending session...");

    LOG_IF_FAILED(pVoukoder->Close(true), "Failed to close Voukoder context.");

    this->isSessionFinished = true;
    this->cvEndSession.notify_all();
    LOG(LL_NFO, "Done.");

    POST();
    return S_OK;
}

// HRESULT Session::createVideoFrames(const int32_t srcWidth, const int32_t srcHeight, const AVPixelFormat srcFmt,
//                                   const int32_t dstWidth, const int32_t dstHeight, const AVPixelFormat dstFmt) {
//    PRE();
//    if (this->isBeingDeleted) {
//        POST();
//        return E_FAIL;
//    }
//    // this->inputFrame = av_frame_alloc();
//    // RET_IF_NULL(this->inputFrame, "Could not allocate video frame", E_FAIL);
//    // this->inputFrame->format = srcFmt;
//    // this->inputFrame->width = srcWidth;
//    // this->inputFrame->height = srcHeight;
//
//    // this->outputFrame = av_frame_alloc();
//    // RET_IF_NULL(this->outputFrame, "Could not allocate video frame", E_FAIL);
//    // this->outputFrame->format = dstFmt;
//    // this->outputFrame->width = dstWidth;
//    // this->outputFrame->height = dstHeight;
//    // av_frame_get_buffer(this->outputFrame, 1);
//    // av_image_alloc(outputFrame->data, outputFrame->linesize, dstWidth, dstHeight, dstFmt, 1);
//    // av_alloc_buff
//
//    this->pSwsContext =
//        sws_getContext(srcWidth, srcHeight, srcFmt, dstWidth, dstHeight, dstFmt, SWS_POINT, nullptr, nullptr,
//        nullptr);
//    POST();
//    return S_OK;
//}

// HRESULT Session::createAudioFrames(const int32_t inputChannels, const AVSampleFormat inputSampleFmt,
//                                   const int32_t inputSampleRate, const int32_t outputChannels,
//                                   const AVSampleFormat outputSampleFmt, const int32_t outputSampleRate) {
//    PRE();
//    if (this->isBeingDeleted) {
//        POST();
//        return E_FAIL;
//    }
//    this->inputAudioFrame = av_frame_alloc();
//    RET_IF_NULL(inputAudioFrame, "Could not allocate input audio frame", E_FAIL);
//    inputAudioFrame->format = inputSampleFmt;
//    inputAudioFrame->sample_rate = inputSampleRate;
//    inputAudioFrame->channels = inputChannels;
//    inputAudioFrame->channel_layout = AV_CH_LAYOUT_STEREO;
//
//    this->outputAudioFrame = av_frame_alloc();
//    RET_IF_NULL(inputAudioFrame, "Could not allocate output audio frame", E_FAIL);
//    outputAudioFrame->format = outputSampleFmt;
//    outputAudioFrame->sample_rate = outputSampleRate;
//    outputAudioFrame->channels = outputChannels;
//    outputAudioFrame->channel_layout = AV_CH_LAYOUT_STEREO;
//
//    this->audioSampleBuffer =
//        av_audio_fifo_alloc(outputSampleFmt, outputChannels,
//                            (this->audioCodecContext->frame_size ? this->audioCodecContext->frame_size : 256) * 4);
//
//    this->pSwrContext = swr_alloc_set_opts(nullptr, AV_CH_LAYOUT_STEREO, outputSampleFmt, outputSampleRate,
//                                           AV_CH_LAYOUT_STEREO, inputSampleFmt, inputSampleRate, AV_LOG_TRACE,
//                                           nullptr);
//
//    // std::stringstream cutoff;
//    // cutoff << std::fixed << std::setprecision(5) << 4.0f * 48000.0f / inputSampleRate;
//
//    // LOG(LL_DBG, cutoff.str());
//    // av_opt_set(this->pSwrContext, "cutoff", cutoff.str().c_str(), 0);
//    /*av_opt_set(this->pSwrContext, "filter_type", "kaiser", 0);
//    av_opt_set(this->pSwrContext, "dither_method", "high_shibata", 0);*/
//    // av_opt_set(this->pSwrContext, "filter_size", "128", 0);
//
//    RET_IF_NULL(this->pSwrContext, "Could not allocate audio resampling context", E_FAIL);
//    RET_IF_FAILED_AV(swr_init(pSwrContext), "Could not initialize audio resampling context", E_FAIL);
//
//    POST();
//    return S_OK;
//}
} // namespace Encoder
