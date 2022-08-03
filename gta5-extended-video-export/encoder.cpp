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
#include <filesystem>
#include <xutility>
#pragma warning(pop)

namespace Encoder {

Session::Session() : videoFrameQueue(16), exrImageQueue(16) {
    PRE();
    LOG(LL_NFO, "Opening session: ", reinterpret_cast<uint64_t>(this));
    POST();
}

Session::~Session() {
    PRE();
    LOG(LL_NFO, "Closing session: ", reinterpret_cast<uint64_t>(this));
    this->isCapturing = false;

    LOG_CALL(LL_DBG, this->finishVideo());
    LOG_CALL(LL_DBG, this->finishAudio());
    LOG_CALL(LL_DBG, this->endSession());

    this->isBeingDeleted = true;

    POST();
}

HRESULT Session::createContext(const VKENCODERCONFIG& config, const std::wstring& inFilename, uint32_t inWidth,
                               uint32_t inHeight, const std::string& inputPixelFmt, uint32_t fps_num, uint32_t fps_den,
                               uint32_t inputChannels, uint32_t inputSampleRate, const std::string& inputSampleFormat,
                               uint32_t inputAlign, bool inExportOpenEXR) {
    PRE();

    this->width = inWidth;
    this->height = inHeight;

    ASSERT_RUNTIME(inFilename.length() < sizeof(VKENCODERINFO::filename) / sizeof(wchar_t), "File name too long.");
    ASSERT_RUNTIME(inputChannels == 1 || inputChannels == 2 || inputChannels == 6,
                   "Invalid number of audio channels, only 1, 2 and 6 (5.1) are supported.");

    Microsoft::WRL::ComPtr<IVoukoder> m_pVoukoder = nullptr;

    REQUIRE(CoCreateInstance(CLSID_CoVoukoder, NULL, CLSCTX_INPROC_SERVER, IID_IVoukoder,
                             reinterpret_cast<void**>(m_pVoukoder.GetAddressOf())),
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

    REQUIRE(m_pVoukoder->Open(vkInfo), "Failed to open Voukoder context.");

    videoFrame = {
        .buffer = new byte*[1], // We'll set this in writeVideoFrame() function
        .rowsize = new int[1],  // We'll set this in writeVideoFrame() function
        .planes = 1,
        .width = static_cast<int>(inWidth),
        .height = static_cast<int>(inHeight),
        .pass = 1,
    };
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

    exportOpenEXR = inExportOpenEXR;

    if (exportOpenEXR) {
        this->exrOutputPath = inFilename + L".OpenEXR";
        std::filesystem::create_directories(this->exrOutputPath);
    }

    pVoukoder = std::move(m_pVoukoder);

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

    struct RGBA {
        half R;
        half G;
        half B;
        half A;
    };

    struct Depth {
        float depth;
    };

    Imf::Header header(this->width, this->height); //, 1, IMATH_NAMESPACE::V2f (0, 0), this->width);
    Imf::FrameBuffer framebuffer;

    if (cRGB != nullptr) {
        LOG_CALL(LL_DBG, header.channels().insert("R", Imf::Channel(Imf::HALF)));
        LOG_CALL(LL_DBG, header.channels().insert("G", Imf::Channel(Imf::HALF)));
        LOG_CALL(LL_DBG, header.channels().insert("B", Imf::Channel(Imf::HALF)));
        LOG_CALL(LL_DBG, header.channels().insert("SSS", Imf::Channel(Imf::HALF)));
        const auto mHDRArray = static_cast<RGBA*>(mHDR.pData);

        if (mHDRArray) {
            LOG_CALL(LL_DBG, framebuffer.insert("R", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].R),
                                                                sizeof(RGBA), sizeof(RGBA) * this->width)));

            LOG_CALL(LL_DBG, framebuffer.insert("G", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].G),
                                                                sizeof(RGBA), sizeof(RGBA) * this->width)));

            LOG_CALL(LL_DBG, framebuffer.insert("B", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].B),
                                                                sizeof(RGBA), sizeof(RGBA) * this->width)));

            LOG_CALL(LL_DBG, framebuffer.insert("SSS", Imf::Slice(Imf::HALF, reinterpret_cast<char*>(&mHDRArray[0].A),
                                                                  sizeof(RGBA), sizeof(RGBA) * this->width)));
        }
    }

    if (cDepth != nullptr) {
        LOG_CALL(LL_DBG, header.channels().insert("depth.Z", Imf::Channel(Imf::FLOAT)));
        const auto mDSArray = static_cast<Depth*>(mDepth.pData);

        LOG_CALL(LL_DBG,
                 framebuffer.insert("depth.Z", Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(&mDSArray[0].depth),
                                                          sizeof(Depth), sizeof(Depth) * this->width)));
    }

    std::vector<uint32_t> stencilBuffer;
    if (cStencil != nullptr) {
        stencilBuffer =
            std::vector<uint32_t>(static_cast<size_t>(mStencil.RowPitch) * static_cast<size_t>(this->height));
        const auto mSArray = static_cast<uint8_t*>(mStencil.pData);

        for (uint32_t i = 0; i < mStencil.RowPitch * this->height; i++) {
            stencilBuffer[i] = static_cast<uint32_t>(mSArray[i]);
        }

        LOG_CALL(LL_DBG, header.channels().insert("objectID", Imf::Channel(Imf::UINT)));

        LOG_CALL(LL_DBG,
                 framebuffer.insert("objectID",
                                    Imf::Slice(Imf::UINT, reinterpret_cast<char*>(stencilBuffer.data()),
                                               sizeof(uint32_t), static_cast<size_t>(mStencil.RowPitch) * size_t{4})));
    }

    std::wstringstream sstream;
    sstream << std::setw(5) << std::setfill(L'0') << this->exrPTS++;
    Imf::OutputFile file(utf8_encode(this->exrOutputPath + L"\\frame." + sstream.str() + L".exr").c_str(), header);

    LOG_CALL(LL_DBG, file.setFrameBuffer(framebuffer));
    LOG_CALL(LL_DBG, file.writePixels(this->height));

    if (cRGB) {
        LOG_CALL(LL_DBG, pDeviceContext->Unmap(cRGB.Get(), 0));
    }

    if (cDepth) {
        LOG_CALL(LL_DBG, pDeviceContext->Unmap(cDepth.Get(), 0));
    }

    if (cStencil) {
        LOG_CALL(LL_DBG, pDeviceContext->Unmap(cStencil.Get(), 0));
    }

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

    POST();
    return S_OK;
}

HRESULT Session::writeVideoFrame(BYTE* pData, const int32_t length, const int rowPitch, const LONGLONG sampleTime) {
    PRE();
    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

    videoFrame.buffer[0] = pData;
    videoFrame.rowsize[0] = rowPitch;

    REQUIRE(pVoukoder->SendVideoFrame(videoFrame), "Failed to send video frame to Voukoder.");

    POST();
    return S_OK;
}

HRESULT Session::writeAudioFrame(BYTE* pData, const int32_t length, LONGLONG sampleTime) {
    PRE();

    if (this->isBeingDeleted) {
        POST();
        return E_FAIL;
    }

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

    if (!this->isAudioFinished) {
        delete this->audioChunk.buffer;
        this->audioChunk.buffer = nullptr;
        this->isAudioFinished = true;
    }

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

} // namespace Encoder
