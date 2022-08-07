#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 26812)
#include "encoder.h"
#include "logger.h"
#include "util.h"

#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfRgba.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <filesystem>
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
    // LOG_CALL(LL_DBG, this->videoFrameQueue.enqueue(Encoder::Session::frameQueueItem(nullptr, 0)));

    // if (thread_video_encoder.joinable()) {
    //     thread_video_encoder.join();
    // }

    // LOG_CALL(LL_DBG, this->exrImageQueue.enqueue(Encoder::Session::exr_queue_item()));

    // if (thread_exr_encoder.joinable()) {
    //     thread_exr_encoder.join();
    // }

    LOG_CALL(LL_DBG, this->finishVideo());
    LOG_CALL(LL_DBG, this->finishAudio());
    LOG_CALL(LL_DBG, this->endSession());
    // LOG_CALL(LL_DBG, pVoukoder->Close(true));
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

HRESULT Session::createContext(const VKENCODERCONFIG& config, const std::wstring& inFilename, uint32_t inWidth,
                               uint32_t inHeight, const std::string& inputPixelFmt, uint32_t fps_num, uint32_t fps_den,
                               uint32_t inputChannels, uint32_t inputSampleRate, const std::string& inputSampleFormat,
                               uint32_t inputAlign, bool inExportOpenExr) {
    PRE();
    // this->oformat = av_guess_format(format.c_str(), nullptr, nullptr);
    // RET_IF_NULL(this->oformat, "Could not find format: " + format, E_FAIL);

    // REQUIRE(this->createVideoContext(inWidth, inHeight, inputPixelFmt, fps_num, fps_den, motionBlurSamples,
    // shutterPosition,
    //                                 outputPixelFmt, vcodec_str, voptions),
    //        "Failed to create video codec context.");
    // REQUIRE(this->createAudioContext(inputChannels, inputSampleRate, inputBitsPerSample, inputSampleFmt, inputAlign,
    //                                 outputSampleFmt, acodec_str, aoptions),
    //        "Failed to create audio codec context.");
    // REQUIRE(this->createFormatContext(format, inFilename, exrOutputPath, fmtOptions), "Failed to create format
    // context.");

    this->width = inWidth;
    this->height = inHeight;

    ASSERT_RUNTIME(inFilename.length() < sizeof(VKENCODERINFO::filename) / sizeof(wchar_t), "File name too long.");
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
            .width = static_cast<int>(inWidth),
            .height = static_cast<int>(inHeight),
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
    inFilename.copy(vkInfo.filename, std::size(vkInfo.filename));
    this->exportExr = inExportOpenExr;
    if (this->exportExr) {
        this->exrOutputPath = utf8_encode(inFilename) + ".OpenEXR";
        std::filesystem::create_directories(this->exrOutputPath);
    }
    // std::copy(inFilename.begin(), inFilename.end(), vkInfo.inFilename);

    REQUIRE(m_pVoukoder->Open(vkInfo), "Failed to open Voukoder context.");

    videoFrame = {
        .buffer = new byte*[1], // We'll set this in writeVideoFrame() function
        .rowsize = new int[1],  // We'll set this in writeVideoFrame() function
        .planes = 1,
        .width = static_cast<int>(inWidth),
        .height = static_cast<int>(inHeight),
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

    // this->thread_video_encoder = std::thread(&Session::videoEncodingThread, this);
    // this->thread_exr_encoder = std::thread(&Session::exrEncodingThread, this);

    this->isCapturing = true;

    POST();
    return S_OK;
}

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

    // this->exrImageQueue.enqueue(exr_queue_item(cRGB, mHDR.pData, cDepth, mDepth.pData, cStencil, mStencil));

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

    if (cRGB != nullptr) {
        LOG_CALL(LL_DBG, header.channels().insert("R", Imf::Channel(Imf::HALF)));
        LOG_CALL(LL_DBG, header.channels().insert("G", Imf::Channel(Imf::HALF)));
        LOG_CALL(LL_DBG, header.channels().insert("B", Imf::Channel(Imf::HALF)));
        LOG_CALL(LL_DBG, header.channels().insert("SSS", Imf::Channel(Imf::HALF)));
        const auto mHDRArray = static_cast<RGBA*>(mHDR.pData);

        LOG_CALL(LL_DBG, framebuffer.insert("R", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].R),
                                                            sizeof(RGBA), sizeof(RGBA) * this->width)));

        LOG_CALL(LL_DBG, framebuffer.insert("G", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].G),
                                                            sizeof(RGBA), sizeof(RGBA) * this->width)));

        LOG_CALL(LL_DBG, framebuffer.insert("B", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].B),
                                                            sizeof(RGBA), sizeof(RGBA) * this->width)));

        LOG_CALL(LL_DBG, framebuffer.insert("SSS", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].A),
                                                              sizeof(RGBA), sizeof(RGBA) * this->width)));
    }

    if (cDepth != nullptr) {
        LOG_CALL(LL_DBG, header.channels().insert("depth.Z", Imf::Channel(Imf::FLOAT)));
        // header.channels().insert("objectID", Imf::Channel(Imf::UINT));
        const auto mDSArray = static_cast<Depth*>(mDepth.pData);

        LOG_CALL(LL_DBG,
                 framebuffer.insert("depth.Z", Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&mDSArray[0].depth),
                                                          sizeof(Depth), sizeof(Depth) * this->width)));
    }

    std::vector<uint32_t> stencilBuffer;
    if (cStencil != nullptr) {
        stencilBuffer =
            std::vector<uint32_t>(static_cast<size_t>(mStencil.RowPitch) * static_cast<size_t>(this->height));
        auto mSArray = static_cast<uint8_t*>(mStencil.pData);

        for (uint32_t i = 0; i < mStencil.RowPitch * this->height; i++) {
            stencilBuffer[i] = static_cast<uint32_t>(mSArray[i]);
        }

        LOG_CALL(LL_DBG, header.channels().insert("objectID", Imf::Channel(Imf::UINT)));

        LOG_CALL(LL_DBG,
                 framebuffer.insert("objectID",
                                    Imf::Slice(Imf::UINT, reinterpret_cast<char*>(stencilBuffer.data()),
                                               sizeof(uint32_t), static_cast<size_t>(mStencil.RowPitch) * size_t{4})));
    }

    // if (CreateDirectoryA(this->exrOutputPath.c_str(), nullptr) || ERROR_ALREADY_EXISTS == GetLastError()) {

    std::stringstream sstream;
    sstream << std::setw(5) << std::setfill('0') << this->exrPTS++;
    Imf::OutputFile file((this->exrOutputPath + "\\frame." + sstream.str() + ".exr").c_str(), header);
    LOG_CALL(LL_DBG, file.setFrameBuffer(framebuffer));
    LOG_CALL(LL_DBG, file.writePixels(this->height));
    //}

    POST();
    return S_OK;
}

HRESULT Session::enqueueVideoFrame(const D3D11_MAPPED_SUBRESOURCE& subresource) {
    PRE();

    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

    LOG(LL_NFO, "Encoding frame: ", this->videoPTS);
    REQUIRE(this->writeVideoFrame(static_cast<BYTE*>(subresource.pData), static_cast<int32_t>(subresource.DepthPitch),
                                  subresource.RowPitch, this->videoPTS++),
            "Failed to write video frame.");

    // const frameQueueItem item(subresource);
    // this->videoFrameQueue.enqueue(item);
    POST();
    return S_OK;
}

// void Session::videoEncodingThread() {
//     PRE();
//     std::lock_guard lock(this->mxEncodingThread);
//     try {
//         int16_t k = 0;
//         bool firstFrame = true;
//         frameQueueItem item = this->videoFrameQueue.dequeue();
//         while (item.subresource != nullptr) {
//             auto& data = *(item.subresource);
//             // if (this->motionBlurSamples == 0) {
//             LOG(LL_NFO, "Encoding frame: ", this->videoPTS);
//             REQUIRE(this->writeVideoFrame(static_cast<BYTE*>(data.pData),
//                                           static_cast<int32_t>(item.subresource->DepthPitch),
//                                           item.subresource->RowPitch, this->videoPTS++),
//                     "Failed to write video frame.");
//             //} else {
//             //    const auto frameRemainder =
//             //        static_cast<uint8_t>(this->motionBlurPTS++ % (static_cast<uint64_t>(this->motionBlurSamples) +
//             //        1));
//             //    const float currentShutterPosition =
//             //        static_cast<float>(frameRemainder) / static_cast<float>(this->motionBlurSamples + 1);
//             //    std::ranges::copy(data, std::begin(this->motionBlurTempBuffer));
//             //    if (frameRemainder == this->motionBlurSamples) {
//             //        // Flush motion blur buffer
//             //        this->motionBlurAccBuffer += this->motionBlurTempBuffer;
//             //        this->motionBlurAccBuffer /= ++k;
//             //        std::ranges::copy(this->motionBlurAccBuffer, std::begin(this->motionBlurDestBuffer));
//             //        REQUIRE(this->writeVideoFrame(std::begin(this->motionBlurDestBuffer),
//             //                                      this->motionBlurDestBuffer.size(), item.rowPitch,
//             this->videoPTS++),
//             //                "Failed to write video frame.");
//             //        k = 0;
//             //        firstFrame = true;
//             //    } else if (currentShutterPosition >= this->shutterPosition) {
//             //        if (firstFrame) {
//             //            // Reset accumulation buffer
//             //            this->motionBlurAccBuffer = this->motionBlurTempBuffer;
//             //            firstFrame = false;
//             //        } else {
//             //            this->motionBlurAccBuffer += this->motionBlurTempBuffer;
//             //        }
//             //        k++;
//             //    }
//             //}
//             item = this->videoFrameQueue.dequeue();
//         }
//     } catch (...) {
//         // Do nothing
//     }
//     this->isEncodingThreadFinished = true;
//     this->cvEncodingThreadFinished.notify_all();
//     POST();
// }

// void Session::exrEncodingThread() {
//     PRE();
//     std::lock_guard<std::mutex> lock(this->mxEXREncodingThread);
//     Imf::setGlobalThreadCount(8);
//     try {
//         exr_queue_item item = this->exrImageQueue.dequeue();
//         while (!item.isEndOfStream) {
//
//             item = this->exrImageQueue.dequeue();
//         }
//     } catch (std::exception& ex) {
//         LOG(LL_ERR, ex.what());
//     }
//     this->isEXREncodingThreadFinished = true;
//     this->cvEXREncodingThreadFinished.notify_all();
//     POST();
// }

HRESULT Session::writeVideoFrame(BYTE* pData, const int32_t length, const int rowPitch, const LONGLONG sampleTime) {
    PRE();
    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

    // Wait until format context is created
    // if (pVoukoder == nullptr) {
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
    // if (pVoukoder == nullptr) {
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
        // if (thread_video_encoder.joinable()) {
        //    // Write end of the stream object with a nullptr
        //    this->videoFrameQueue.enqueue(frameQueueItem(nullptr));
        //    std::unique_lock<std::mutex> lock(this->mxEncodingThread);
        //    while (!this->isEncodingThreadFinished) {
        //        this->cvEncodingThreadFinished.wait(lock);
        //    }

        //    thread_video_encoder.join();
        //}

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
