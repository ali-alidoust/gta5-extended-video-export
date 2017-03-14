#include "encoder.h"
#include "logger.h"
#include <ImfHeader.h>
#include <ImfFloatAttribute.h>
#include <ImfChannelList.h>
#include <ImfIO.h>
#include <ImfOutputFile.h>
#include <ImfRgbaFile.h>
#include <ImfRgba.h>
#include <fstream>


namespace Encoder {
	
	const AVRational MF_TIME_BASE = { 1, 10000000 };

	Session::Session() :
		thread_video_encoder(),
		videoFrameQueue(16),
		exrImageQueue(16)
	{
		PRE();
		LOG(LL_NFO, "Opening session: ", (uint64_t)this);
		POST();
	}

	Session::~Session() {
		PRE();
		LOG(LL_NFO, "Closing session: ", (uint64_t)this);
		this->isCapturing = false;
		LOG_CALL(LL_DBG, this->videoFrameQueue.enqueue(Encoder::Session::frameQueueItem(nullptr)));

		if (thread_video_encoder.joinable()) {
			thread_video_encoder.join();
		}

		LOG_CALL(LL_DBG, this->exrImageQueue.enqueue(Encoder::Session::exr_queue_item()));
		
		if (thread_exr_encoder.joinable()) {
			thread_exr_encoder.join();
		}

		LOG_CALL(LL_DBG, this->finishVideo());
		LOG_CALL(LL_DBG, this->finishAudio());
		LOG_CALL(LL_DBG, this->endSession());
		this->isBeingDeleted = true;


		LOG_CALL(LL_DBG, av_free(this->oformat));
		LOG_CALL(LL_DBG, av_free(this->fmtContext));
		LOG_CALL(LL_DBG, avcodec_close(this->videoCodecContext));
		LOG_CALL(LL_DBG, av_free(this->videoCodecContext));
		LOG_CALL(LL_DBG, av_free(this->inputFrame));
		LOG_CALL(LL_DBG, av_free(this->outputFrame));
		LOG_CALL(LL_DBG, sws_freeContext(this->pSwsContext));
		LOG_CALL(LL_DBG, swr_free(&this->pSwrContext));
		if (this->videoOptions) {
			LOG_CALL(LL_DBG, av_dict_free(&this->videoOptions));
		}
		if (this->audioOptions) {
			LOG_CALL(LL_DBG, av_dict_free(&this->audioOptions));
		}
		if (this->fmtOptions) {
			LOG_CALL(LL_DBG, av_dict_free(&this->fmtOptions));
		}
		LOG_CALL(LL_DBG, avcodec_close(this->audioCodecContext));
		LOG_CALL(LL_DBG, av_free(this->audioCodecContext));
		LOG_CALL(LL_DBG, av_free(this->inputAudioFrame));
		LOG_CALL(LL_DBG, av_free(this->outputAudioFrame));
		LOG_CALL(LL_DBG, av_audio_fifo_free(this->audioSampleBuffer));
		POST();
	}

	HRESULT Session::createContext(std::string format, std::string filename, std::string exrOutputPath, std::string fmtOptions, uint64_t width, uint64_t height, std::string inputPixelFmt, uint32_t fps_num, uint32_t fps_den, uint8_t motionBlurSamples, float shutterPosition, std::string outputPixelFmt, std::string vcodec_str, std::string voptions, uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample, std::string inputSampleFmt, uint32_t inputAlign, std::string outputSampleFmt, std::string acodec_str, std::string aoptions)
	{
		this->oformat = av_guess_format(format.c_str(), NULL, NULL);
		RET_IF_NULL(this->oformat, "Could find format: " + format, E_FAIL);

		REQUIRE(this->createVideoContext(width, height, inputPixelFmt, fps_num, fps_den, motionBlurSamples, shutterPosition, outputPixelFmt, vcodec_str, voptions), "Failed to create video codec context.");
		REQUIRE(this->createAudioContext(inputChannels, inputSampleRate, inputBitsPerSample, inputSampleFmt, inputAlign, outputSampleFmt, acodec_str, aoptions), "Failed to create audio codec context.");
		REQUIRE(this->createFormatContext(format, filename, exrOutputPath, fmtOptions), "Failed to create format context.");
		return S_OK;
	}

	HRESULT Session::createVideoContext(UINT width, UINT height, std::string inputPixelFormatString, UINT fps_num, UINT fps_den, uint8_t motionBlurSamples, float shutterPosition, std::string outputPixelFormatString, std::string vcodec, std::string preset)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		if (vcodec.empty()) {
			this->videoCodecContext = nullptr;
			this->isVideoContextCreated = true;
			POST();
			return S_OK;
		}

		LOG(LL_NFO, "Creating video context:");
		LOG(LL_NFO, "  encoder: ", vcodec);
		LOG(LL_NFO, "  options: ", preset);

		this->inputPixelFormat = av_get_pix_fmt(inputPixelFormatString.c_str());
		if (this->inputPixelFormat == AV_PIX_FMT_NONE) {
			LOG(LL_ERR, "Unknown input pixel format specified: ", inputPixelFormatString);
			POST();
			return E_FAIL;
		}

		this->outputPixelFormat = av_get_pix_fmt(outputPixelFormatString.c_str());
		if (this->outputPixelFormat == AV_PIX_FMT_NONE) {
			LOG(LL_ERR, "Unknown output pixel format specified: ", outputPixelFormatString);
			POST();
			return E_FAIL;
		}

		this->width = width;
		this->height = height;
		this->motionBlurSamples = motionBlurSamples;
		this->motionBlurAccBuffer = std::valarray<uint16_t>(width * height * 4);
		this->motionBlurTempBuffer = std::valarray<uint16_t>(width * height * 4);
		this->motionBlurDestBuffer = std::valarray<uint8_t>(width * height * 4);
		this->shutterPosition = shutterPosition;

		//this->audioSampleRateMultiplier = ((float)fps_num * ((float)motionBlurSamples + 1)) / ((float)fps_den * 60.0f);


		this->videoCodec = avcodec_find_encoder_by_name(vcodec.c_str());
		RET_IF_NULL(this->videoCodec, "Could not find video codec:" + vcodec, E_FAIL);

		this->videoCodecContext = avcodec_alloc_context3(this->videoCodec);
		RET_IF_NULL(this->videoCodecContext, "Could not allocate context for the video codec", E_FAIL);

		av_dict_parse_string(&this->videoOptions, preset.c_str(), "=", "/", 0);
		//av_set_options_string(this->videoCodecContext, preset.c_str(), "=", "/");
		
		RET_IF_FAILED(this->createVideoFrames(width, height, this->inputPixelFormat, width, height, this->outputPixelFormat), "Could not create video frames", E_FAIL);

		this->videoCodecContext->codec_id = this->videoCodec->id;
		this->videoCodecContext->pix_fmt = this->outputPixelFormat;
		this->videoCodecContext->width = width;
		this->videoCodecContext->height = height;
		this->videoCodecContext->time_base = av_make_q(fps_den, fps_num);
		this->videoCodecContext->framerate = av_make_q(fps_num, fps_den);
		this->videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

		if (this->oformat->flags & AVFMT_GLOBALHEADER)
		{
			this->videoCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
		
		RET_IF_FAILED_AV(avcodec_open2(this->videoCodecContext, this->videoCodec, &this->videoOptions), "Could not open video codec", E_FAIL);
		
		this->thread_video_encoder = std::thread(&Session::videoEncodingThread, this);
		this->thread_exr_encoder = std::thread(&Session::exrEncodingThread, this);

		LOG(LL_NFO, "Video context was created successfully.");
		this->isVideoContextCreated = true;
		POST();
		return S_OK;
	}

	HRESULT Session::createAudioContext(uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample, std::string inputSampleFormat, uint32_t inputAlignment, std::string outputSampleFormatString, std::string acodec, std::string preset) {
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		if (acodec.empty()) {
			this->audioCodecContext = nullptr;
			this->isAudioContextCreated = true;
			POST();
			return S_OK;
		}

		LOG(LL_NFO, "Creating audio context:");
		LOG(LL_NFO, "  encoder: ", acodec);
		LOG(LL_NFO, "  options: ", preset);
		//AVCodecID audioCodecId = AV_CODEC_ID_PCM_S16LE;

		//this->inputAudioSampleRate = static_cast<uint32_t>(inputSampleRate * this->audioSampleRateMultiplier);
		this->inputAudioSampleRate = inputSampleRate;
		this->inputAudioChannels = inputChannels;
		this->outputAudioChannels = inputChannels; // FIXME

		this->inputAudioSampleFormat = av_get_sample_fmt(inputSampleFormat.c_str());
		if (this->inputAudioSampleFormat == AV_SAMPLE_FMT_NONE) {
			LOG(LL_ERR, "Unknown audio sample format specified: ", inputSampleFormat);
			POST();
			return E_FAIL;
		}

		this->outputAudioSampleFormat = av_get_sample_fmt(outputSampleFormatString.c_str());
		if (this->outputAudioSampleFormat == AV_SAMPLE_FMT_NONE) {
			LOG(LL_ERR, "Unknown audio sample format specified: ", outputSampleFormatString);
			POST();
			return E_FAIL;
		}

		this->audioCodec = avcodec_find_encoder_by_name(acodec.c_str());
		RET_IF_NULL(this->audioCodec, "Could not find audio codec:" + acodec, E_FAIL);
		this->audioCodecContext = avcodec_alloc_context3(this->audioCodec);
		RET_IF_NULL(this->audioCodecContext, "Could not allocate context for the audio codec", E_FAIL);
		
		//av_set_options_string(this->audioCodecContext, preset.c_str(), "=", "/");

		this->audioCodecContext->codec_id = this->audioCodec->id;
		this->audioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
		this->audioCodecContext->channels = inputChannels;
		this->audioCodecContext->sample_fmt = this->outputAudioSampleFormat;
		this->audioCodecContext->time_base = { 1, (int)inputSampleRate };
		//this->audioCodecContext->frame_size = 256;
		this->audioCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
		
		if (this->oformat->flags & AVFMT_GLOBALHEADER)
		{
			this->audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
		av_dict_parse_string(&this->audioOptions, preset.c_str(), "=", "/", 0);
		this->audioBlockAlign = inputAlignment;

		RET_IF_FAILED_AV(avcodec_open2(this->audioCodecContext, this->audioCodec, &this->audioOptions), "Could not open audio codec", E_FAIL);

		this->outputAudioSampleRate = this->audioCodecContext->sample_rate;
		LOG(LL_TRC, this->outputAudioSampleRate);
		RET_IF_FAILED(this->createAudioFrames(inputChannels, this->inputAudioSampleFormat, this->inputAudioSampleRate, inputChannels, this->outputAudioSampleFormat, this->outputAudioSampleRate), "Could not create audio frames", E_FAIL); 

		

		LOG(LL_NFO, "Audio context was created successfully.");
		this->isAudioContextCreated = true;

		POST();
		return S_OK;
	}

	HRESULT Session::createFormatContext(std::string format, std::string filename, std::string exrOutputPath, std::string fmtPreset)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		std::lock_guard<std::mutex> guard(this->mxFormatContext);

		this->exrOutputPath = exrOutputPath;

		this->filename = filename;
		LOG(LL_NFO, "Exporting to file: ", this->filename);

		/*this->oformat = av_guess_format(format.c_str(), NULL, NULL);
		RET_IF_NULL(this->oformat, "Could not create format", E_FAIL);*/
		RET_IF_FAILED_AV(avformat_alloc_output_context2(&this->fmtContext, this->oformat, NULL, NULL), "Could not allocate format context", E_FAIL);
		RET_IF_NULL(this->fmtContext, "Could not allocate format context", E_FAIL);
		//this->fmtContext = avformat_alloc_context();

		this->fmtContext->oformat = this->oformat;
		/*if (this->videoCodec) {
			this->fmtContext->video_codec_id = this->videoCodec->id;
		}
		if (this->audioCodec) {
			this->fmtContext->audio_codec_id = this->audioCodec->id;
		}*/

		//this->fmtContext->oformat = this->oformat;

		/*if (this->fmtContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			if (this->videoCodecContext) {
				this->videoCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
			if (this->audioCodecContext) {
				this->audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
		}*/

		//av_set_options_string(this->fmtContext, fmtPreset.c_str(), "=", "/");
		av_dict_parse_string(&this->fmtOptions, fmtPreset.c_str(), "=", "/", 0);

		//av_opt_set(this->fmtContext->priv_data, "path", filename, 0);
		strcpy_s(this->fmtContext->filename, this->filename.c_str());
		//av_opt_set(this->fmtContext, "filename", this->filename.c_str(), 0);

		bool hasVideo = false;
		bool hasAudio = false;

		if (this->videoCodecContext && (!this->oformat->query_codec || this->oformat->query_codec(this->videoCodec->id, 0) == 1)) {
			this->videoStream = avformat_new_stream(this->fmtContext, this->videoCodec);
			RET_IF_NULL(this->videoStream, "Could not create video stream", E_FAIL);
			avcodec_parameters_from_context(this->videoStream->codecpar, this->videoCodecContext);
			//avcodec_copy_context(this->videoStream->codec, this->videoCodecContext);
			this->videoStream->time_base = this->videoCodecContext->time_base;
			//this->videoStream->index = this->fmtContext->nb_streams - 1;
			hasVideo = true;
		}

		if (this->audioCodecContext && (!this->oformat->query_codec || this->oformat->query_codec(this->audioCodec->id, 0) == 1)) {
			this->audioStream = avformat_new_stream(this->fmtContext, this->audioCodec);
			RET_IF_NULL(this->audioStream, "Could not create audio stream", E_FAIL);
			avcodec_parameters_from_context(this->audioStream->codecpar, this->audioCodecContext);
			//avcodec_copy_context(this->audioStream->codec, this->audioCodecContext);
			this->audioStream->time_base = this->audioCodecContext->time_base;
			//this->audioStream->index = this->fmtContext->nb_streams - 1;
			hasAudio = true;
		}

		if (!hasAudio && !hasVideo) {
			LOG(LL_NFO, "Both audio and video are disabled. Cannot create format context");
			this->isCapturing = true;
			this->isFormatContextCreated = true;
			this->cvFormatContext.notify_all();
			POST();
			return E_FAIL;
		}

		RET_IF_FAILED_AV(avio_open(&this->fmtContext->pb, filename.c_str(), AVIO_FLAG_WRITE), "Could not open output file", E_FAIL);
		RET_IF_NULL(this->fmtContext->pb, "Could not open output file", E_FAIL);
		RET_IF_FAILED_AV(avformat_write_header(this->fmtContext, &this->fmtOptions), "Could not write header", E_FAIL);
		LOG(LL_NFO, "Format context was created successfully.");
		this->isCapturing = true;
		this->isFormatContextCreated = true;
		this->cvFormatContext.notify_all();

		POST();
		return S_OK;
	}

	HRESULT Session::enqueueEXRImage(ComPtr<ID3D11DeviceContext> pDeviceContext, ComPtr<ID3D11Texture2D> cRGB, ComPtr<ID3D11Texture2D> cDepth, ComPtr<ID3D11Texture2D> cStencil) {
		PRE();

		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		D3D11_MAPPED_SUBRESOURCE mHDR = { 0 };
		D3D11_MAPPED_SUBRESOURCE mDepth = { 0 };
		D3D11_MAPPED_SUBRESOURCE mStencil = { 0 };

		if (cRGB) {
			REQUIRE(pDeviceContext->Map(cRGB.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &mHDR), "Failed to map HDR texture");
		}

		if (cDepth) { 
			REQUIRE(pDeviceContext->Map(cDepth.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &mDepth), "Failed to map depth texture");
		}

		if (cStencil) {
			REQUIRE(pDeviceContext->Map(cStencil.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &mStencil), "Failed to map stencil texture");
		}

		this->exrImageQueue.enqueue(exr_queue_item(cRGB, mHDR.pData, cDepth, mDepth.pData, cStencil, mStencil));

		POST();
		return S_OK;
	}

	HRESULT Session::enqueueVideoFrame(BYTE *pData, int length) {
		PRE();

		if (!this->videoCodecContext) {
			POST();
			return S_OK;
		}

		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		auto pVector = std::shared_ptr<std::valarray<uint8_t>>(new std::valarray<uint8_t>(length));

		std::copy(pData, pData + length, std::begin(*pVector));

		frameQueueItem item(pVector);
		this->videoFrameQueue.enqueue(item);
		POST();
		return S_OK;
	}

	void Session::videoEncodingThread() {
		PRE();
		std::lock_guard<std::mutex> lock(this->mxEncodingThread);
		int k=0;
		bool firstFrame;
		try {
			frameQueueItem item = this->videoFrameQueue.dequeue();
			while (item.data != nullptr) {
				auto& data = *(item.data);
				if (this->motionBlurSamples == 0) {
					LOG(LL_NFO, "Encoding frame: ", this->videoPTS);
					REQUIRE(this->writeVideoFrame(std::begin(data), item.data->size(), this->videoPTS++), "Failed to write video frame.");
				} else {
					int frameRemainder = this->motionBlurPTS++ % (this->motionBlurSamples + 1);
					float currentShutterPosition = (float)frameRemainder / ((float)this->motionBlurSamples + 1);
					std::copy(std::begin(data), std::end(data), std::begin(this->motionBlurTempBuffer));
					if (frameRemainder == this->motionBlurSamples) {
						// Flush motion blur buffer
						this->motionBlurAccBuffer += this->motionBlurTempBuffer;
						this->motionBlurAccBuffer /= ++k;
						std::copy(std::begin(this->motionBlurAccBuffer), std::end(this->motionBlurAccBuffer), std::begin(this->motionBlurDestBuffer));
						REQUIRE(this->writeVideoFrame(std::begin(this->motionBlurDestBuffer), this->motionBlurDestBuffer.size(), this->videoPTS++), "Failed to write video frame");
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

	void Session::exrEncodingThread()
	{
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
					RGBA* mHDRArray = (RGBA*)item.pRGBData;

					LOG_CALL(LL_DBG, framebuffer.insert("R",
						Imf::Slice(
							Imf::HALF,
							(char*)&mHDRArray[0].R,
							sizeof(RGBA),
							sizeof(RGBA) * this->width
							)));

					LOG_CALL(LL_DBG, framebuffer.insert("G",
						Imf::Slice(
							Imf::HALF,
							(char*)&mHDRArray[0].G,
							sizeof(RGBA),
							sizeof(RGBA) * this->width
							)));

					LOG_CALL(LL_DBG, framebuffer.insert("B",
						Imf::Slice(
							Imf::HALF,
							(char*)&mHDRArray[0].B,
							sizeof(RGBA),
							sizeof(RGBA) * this->width
							)));

					LOG_CALL(LL_DBG, framebuffer.insert("SSS",
						Imf::Slice(
							Imf::HALF,
							(char*)&mHDRArray[0].A,
							sizeof(RGBA),
							sizeof(RGBA) * this->width
							)));
				}
				
				if (item.cDepth != nullptr) {
					LOG_CALL(LL_DBG, header.channels().insert("depth.Z", Imf::Channel(Imf::FLOAT)));
					//header.channels().insert("objectID", Imf::Channel(Imf::UINT));
					Depth* mDSArray = (Depth*)item.pDepthData;

					LOG_CALL(LL_DBG, framebuffer.insert("depth.Z",
						Imf::Slice(
							Imf::FLOAT,
							(char*)&mDSArray[0].depth,
							sizeof(Depth),
							sizeof(Depth) * this->width
							)));
				}

				std::vector<uint32_t> stencilBuffer;
				if (item.cStencil != nullptr) {
					stencilBuffer = std::vector<uint32_t>(item.mStencilData.RowPitch * this->height);
					uint8_t* mSArray = (uint8_t*)item.mStencilData.pData;

					for (uint32_t i = 0; i < item.mStencilData.RowPitch * this->height; i++) {
						stencilBuffer[i] = static_cast<uint32_t>(mSArray[i]);
					}

					LOG_CALL(LL_DBG, header.channels().insert("objectID", Imf::Channel(Imf::UINT)));

					LOG_CALL(LL_DBG, framebuffer.insert("objectID",
						Imf::Slice(
							Imf::UINT,
							(char*)stencilBuffer.data(),
							sizeof(uint32_t),
							item.mStencilData.RowPitch * 4
							)));
				}

				if (CreateDirectoryA(this->exrOutputPath.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError()) {


					std::stringstream sstream;
					sstream << std::setw(5) << std::setfill('0') << this->exrPTS++;
					Imf::OutputFile file((this->exrOutputPath + "\\frame" +  sstream.str() + ".exr").c_str(), header);
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

	HRESULT Session::writeVideoFrame(BYTE *pData, size_t length, LONGLONG sampleTime) {
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		// Wait until format context is created
		{
			std::unique_lock<std::mutex> lk(this->mxFormatContext);
			while (!isFormatContextCreated) {
				this->cvFormatContext.wait_for(lk, std::chrono::milliseconds(1));
			}
		}


		int bufferLength = av_image_get_buffer_size(this->inputPixelFormat, this->width, this->height, 1);
		if (length != bufferLength) {
			LOG(LL_ERR, "IMFSample buffer size != av_image_get_buffer_size: ", length, " vs ", bufferLength);
			POST();
			return E_FAIL;
		}

		AVFrame* inputFrame = av_frame_alloc();
		RET_IF_NULL(inputFrame, "Could not allocate video frame", E_FAIL);
		inputFrame->format = this->inputPixelFormat;
		inputFrame->width = this->width;
		inputFrame->height = this->height;

		AVFrame* outputFrame = av_frame_alloc();
		RET_IF_NULL(outputFrame, "Could not allocate video frame", E_FAIL);
		outputFrame->format = this->outputPixelFormat;
		outputFrame->width = this->width;
		outputFrame->height = this->height;
		av_frame_get_buffer(outputFrame, 1);
		//REQUIRE(av_frame_make_writable(inputFrame), "inputFrame is not writable");

		RET_IF_FAILED(av_image_fill_arrays(inputFrame->data, inputFrame->linesize, pData, this->inputPixelFormat, this->width, this->height, 1), "Could not fill the frame with data from the buffer", E_FAIL);

		//RET_IF_FAILED(av_frame_make_writable(outputFrame), "outputFrame is not writable", E_FAIL);

		sws_scale(pSwsContext, inputFrame->data, inputFrame->linesize, 0, this->height, outputFrame->data, outputFrame->linesize);

		av_frame_unref(inputFrame);
		av_frame_free(&inputFrame);
		//outputFrame->pts = av_rescale_q(sampleTime, this->videoCodecContext->time_base, this->videoStream->time_base);
		outputFrame->pts = sampleTime;

		std::shared_ptr<AVPacket> pPkt(new AVPacket(), av_packet_unref);

		av_init_packet(pPkt.get());
		pPkt->data = NULL;
		pPkt->size = 0;

		//int got_packet;
		//avcodec_encode_video2(this->videoCodecContext, pPkt.get(), outputFrame, &got_packet);

		avcodec_send_frame(this->videoCodecContext, outputFrame);
		
		if (SUCCEEDED(avcodec_receive_packet(this->videoCodecContext, pPkt.get()))) {
			std::lock_guard<std::mutex> guard(this->mxWriteFrame);
			av_packet_rescale_ts(pPkt.get(), this->videoCodecContext->time_base, this->videoStream->time_base);
			pPkt->stream_index = this->videoStream->index;
			av_interleaved_write_frame(this->fmtContext, pPkt.get());
		}

		//av_frame_unref()
		av_frame_unref(outputFrame);
		av_frame_free(&outputFrame);
		//av_frame_free(&inputFrame);


		POST();
		return S_OK;
	}

	HRESULT Session::writeAudioFrame(BYTE *pData, size_t length, LONGLONG sampleTime)
	{
		PRE();

		if (!this->audioCodecContext) {
			POST();
			return S_OK;
		}

		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		// Wait until format context is created
		{
			std::unique_lock<std::mutex> lk(this->mxFormatContext);
			while (!isFormatContextCreated) {
				this->cvFormatContext.wait_for(lk, std::chrono::milliseconds(1));
			}
		}
		
		int64_t numSamples = length / av_samples_get_buffer_size(NULL, this->inputAudioFrame->channels, 1, (AVSampleFormat)this->inputAudioFrame->format, this->audioBlockAlign);

		this->inputAudioFrame->nb_samples = static_cast<int>(numSamples);

		//int64_t numOutSamples = av_rescale_rnd(swr_get_delay(this->pSwrContext, this->inputAudioSampleRate) + numSamples, this->outputAudioSampleRate, this->inputAudioSampleRate, AV_ROUND_UP);
		/*int numOutSamples = swr_get_out_samples(this->pSwrContext, this->inputAudioFrame->nb_samples);
		if (numOutSamples < 0) {
			RET_IF_FAILED_AV(numOutSamples, "Failed to calculate number of output samples.", E_FAIL);
		}

		if (!numOutSamples) {
			return S_OK;
		}*/

		int frameSize = this->audioCodecContext->frame_size ? this->audioCodecContext->frame_size : 256;
		this->outputAudioFrame->nb_samples = frameSize;//static_cast<int>(numOutSamples);
		
		avcodec_fill_audio_frame(this->inputAudioFrame, this->inputAudioFrame->channels, (AVSampleFormat)this->inputAudioFrame->format, pData, static_cast<int>(length), this->audioBlockAlign);
		RET_IF_FAILED_AV(swr_convert_frame(this->pSwrContext, this->outputAudioFrame, this->inputAudioFrame), "Failed to convert audio frame", E_FAIL);
		
		//swr_convert()

		if (!this->outputAudioFrame->nb_samples) {
			POST();
			return S_OK;
		}

		av_audio_fifo_write(this->audioSampleBuffer, (void**)this->outputAudioFrame->data, this->outputAudioFrame->nb_samples);


		if (av_audio_fifo_size(this->audioSampleBuffer) < frameSize) {
			POST();
			return S_OK;
		}
		

		int bufferSize = av_samples_get_buffer_size(NULL, this->outputAudioChannels, frameSize, this->outputAudioSampleFormat, 0);
		uint8_t* localBuffer = new uint8_t[bufferSize];
		AVFrame* localFrame = av_frame_alloc();
		localFrame->channel_layout = AV_CH_LAYOUT_STEREO;
		localFrame->format = this->outputAudioSampleFormat;
		localFrame->nb_samples = frameSize;
		avcodec_fill_audio_frame(localFrame, this->outputAudioChannels, this->outputAudioSampleFormat, localBuffer, bufferSize, 0);
		av_audio_fifo_read(this->audioSampleBuffer, (void**)localFrame->data, frameSize);


		localFrame->pts = this->audioPTS;
		//this->audioPTS += localFrame->nb_samples;
		//this->outputAudioFrame->pts = this->audioPTS;
		this->audioPTS += frameSize;

		std::shared_ptr<AVPacket> pPkt(new AVPacket(), av_packet_unref);

		av_init_packet(pPkt.get());
		pPkt->data = NULL;
		pPkt->size = 0;

		avcodec_send_frame(this->audioCodecContext, localFrame);
		delete[] localBuffer;
		av_frame_free(&localFrame);
		if (SUCCEEDED(avcodec_receive_packet(this->audioCodecContext, pPkt.get()))) {
			std::lock_guard<std::mutex> guard(this->mxWriteFrame);
			av_packet_rescale_ts(pPkt.get(), this->audioCodecContext->time_base, this->audioStream->time_base);
			pPkt->stream_index = this->audioStream->index;
			av_interleaved_write_frame(this->fmtContext, pPkt.get());
		}

		POST();
		return S_OK;
	}

	HRESULT Session::finishVideo()
	{
		PRE();
		std::lock_guard<std::mutex> guard(this->mxFinish);
		if (!this->videoCodecContext || this->isVideoFinished || !this->isVideoContextCreated || this->isBeingDeleted) {
			this->isVideoFinished = true;
			POST();
			return S_OK;
		}

		// Wait until the video encoding thread is finished.
		{
			// Write end of the stream object with a nullptr
			this->videoFrameQueue.enqueue(frameQueueItem(nullptr));
			std::unique_lock<std::mutex> lock(this->mxEncodingThread);
			while (!this->isEncodingThreadFinished) {
				this->cvEncodingThreadFinished.wait(lock);
			}
		}

		// Wait until the depth encoding thread is finished
		{
			// Write end of the stream object with a nullptr
			this->exrImageQueue.enqueue(exr_queue_item());
			std::unique_lock<std::mutex> lock(this->mxEXREncodingThread);
			while (!this->isEXREncodingThreadFinished) {
				this->cvEXREncodingThreadFinished.wait(lock);
			}
		}


		if (thread_video_encoder.joinable()) {
			thread_video_encoder.join();
		}

		if (thread_exr_encoder.joinable()) {
			thread_exr_encoder.join();
		}

		// Write delayed frames
		{
			AVPacket pkt;

			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;

			//int got_packet;
			//avcodec_encode_video2(this->videoCodecContext, &pkt, NULL, &got_packet);
			avcodec_send_frame(this->videoCodecContext, NULL);
			while (SUCCEEDED(avcodec_receive_packet(this->videoCodecContext, &pkt))) {
				std::lock_guard<std::mutex> guard(this->mxWriteFrame);
				av_packet_rescale_ts(&pkt, this->videoCodecContext->time_base, this->videoStream->time_base);
				//pkt.dts = outputFrame->pts;
				pkt.stream_index = this->videoStream->index;
				av_interleaved_write_frame(this->fmtContext, &pkt);
				//avcodec_encode_video2(this->videoCodecContext, &pkt, NULL, &got_packet);
			}

			av_packet_unref(&pkt);
		}

		this->isVideoFinished = true;

		POST();
		return S_OK;
	}

	HRESULT Session::finishAudio()
	{
		PRE();
		std::lock_guard<std::mutex> guard(this->mxFinish);
		if (!this->audioCodecContext || this->isAudioFinished || !this->isAudioContextCreated || this->isBeingDeleted) {
			this->isAudioFinished = true;
			POST();
			return S_OK;
		}

		// Write delated frames
		{

			LOG(LL_WRN, "FIXME: Audio samples discarded:", av_audio_fifo_size(this->audioSampleBuffer));

			//AVPacket pkt;

			//av_init_packet(&pkt);
			//pkt.data = NULL;
			//pkt.size = 0;

			//int got_packet;
			//avcodec_encode_audio2(this->audioCodecContext, &pkt, NULL, &got_packet);
			//while (got_packet != 0) {
			//	std::lock_guard<std::mutex> guard(this->mxWriteFrame);
			//	//av_packet_rescale_ts(&pkt, this->audioCodecContext->time_base, this->audioStream->time_base);
			//	pkt.stream_index = this->audioStream->index;
			//	av_interleaved_write_frame(this->fmtContext, &pkt);
			//	avcodec_encode_audio2(this->audioCodecContext, &pkt, NULL, &got_packet);
			//}

			//av_free_packet(&pkt);
			//av_packet_unref(&pkt);
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

		//this->videoFrameQueue.enqueue(Encoder::Session::frameQueueItem(nullptr, -1));

		while (true) {
			std::lock_guard<std::mutex> guard(this->mxFinish);
			if (this->isVideoFinished && this->isAudioFinished) {
				break;
			}
		}
		
		this->isCapturing = false;
		LOG(LL_NFO, "Ending session...");

		LOG(LL_NFO, "Closing files...");
		LOG_IF_FAILED_AV(av_write_trailer(this->fmtContext), "Could not finalize the output file.");
		LOG_IF_FAILED_AV(avcodec_close(this->videoCodecContext), "Could not close the video codec.");
		LOG_IF_FAILED_AV(avcodec_close(this->audioCodecContext), "Could not close the audio codec.");
		LOG_IF_FAILED_AV(avio_close(this->fmtContext->pb), "Could not close the output file.");
		/*av_free(this->videoCodecContext.get());
		av_free(this->audioCodecContext.get());*/
		
		this->isSessionFinished = true;
		this->cvEndSession.notify_all();
		LOG(LL_NFO, "Done.");

		POST();
		return S_OK;
	}
	HRESULT Session::createVideoFrames(uint32_t srcWidth, uint32_t srcHeight, AVPixelFormat srcFmt, uint32_t dstWidth, uint32_t dstHeight, AVPixelFormat dstFmt)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		this->inputFrame = av_frame_alloc();
		RET_IF_NULL(this->inputFrame, "Could not allocate video frame", E_FAIL);
		this->inputFrame->format = srcFmt;
		this->inputFrame->width = srcWidth;
		this->inputFrame->height = srcHeight;

		this->outputFrame = av_frame_alloc();
		RET_IF_NULL(this->outputFrame, "Could not allocate video frame", E_FAIL);
		this->outputFrame->format = dstFmt;
		this->outputFrame->width = dstWidth;
		this->outputFrame->height = dstHeight;
		av_frame_get_buffer(this->outputFrame, 1);
		//av_image_alloc(outputFrame->data, outputFrame->linesize, dstWidth, dstHeight, dstFmt, 1);
		//av_alloc_buff

		this->pSwsContext = sws_getContext(srcWidth, srcHeight, srcFmt, dstWidth, dstHeight, dstFmt, SWS_POINT, NULL, NULL, NULL);
		POST();
		return S_OK;
	}

	HRESULT Session::createAudioFrames(uint32_t inputChannels, AVSampleFormat inputSampleFmt, uint32_t inputSampleRate, uint32_t outputChannels, AVSampleFormat outputSampleFmt, uint32_t outputSampleRate)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		this->inputAudioFrame = av_frame_alloc();
		RET_IF_NULL(inputAudioFrame, "Could not allocate input audio frame", E_FAIL);
		inputAudioFrame->format = inputSampleFmt;
		inputAudioFrame->sample_rate = inputSampleRate;
		inputAudioFrame->channels = inputChannels;
		inputAudioFrame->channel_layout = AV_CH_LAYOUT_STEREO;

		this->outputAudioFrame = av_frame_alloc();
		RET_IF_NULL(inputAudioFrame, "Could not allocate output audio frame", E_FAIL);
		outputAudioFrame->format = outputSampleFmt;
		outputAudioFrame->sample_rate = outputSampleRate;
		outputAudioFrame->channels = outputChannels;
		outputAudioFrame->channel_layout = AV_CH_LAYOUT_STEREO;

		this->audioSampleBuffer = av_audio_fifo_alloc(outputSampleFmt, outputChannels, (this->audioCodecContext->frame_size ? this->audioCodecContext->frame_size : 256) * 4);


		this->pSwrContext = swr_alloc_set_opts(NULL,
			AV_CH_LAYOUT_STEREO,
			outputSampleFmt,
			outputSampleRate,
			AV_CH_LAYOUT_STEREO,
			inputSampleFmt,
			inputSampleRate,
			AV_LOG_TRACE, NULL);

		//std::stringstream cutoff;
		//cutoff << std::fixed << std::setprecision(5) << 4.0f * 48000.0f / inputSampleRate;

		//LOG(LL_DBG, cutoff.str());
		//av_opt_set(this->pSwrContext, "cutoff", cutoff.str().c_str(), 0);
		/*av_opt_set(this->pSwrContext, "filter_type", "kaiser", 0);
		av_opt_set(this->pSwrContext, "dither_method", "high_shibata", 0);*/
		//av_opt_set(this->pSwrContext, "filter_size", "128", 0);

		RET_IF_NULL(this->pSwrContext, "Could not allocate audio resampling context", E_FAIL);
		RET_IF_FAILED_AV(swr_init(pSwrContext), "Could not initialize audio resampling context", E_FAIL);

		POST();
		return S_OK;
	}
}
