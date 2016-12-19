#include "stdafx.h"
#include "encoder.h"
#include "logger.h"


namespace Encoder {
	
	const AVRational MF_TIME_BASE = { 1, 10000000 };

	Session::Session() :
		thread_video_encoder(),
		videoFrameQueue(16)
	{
		PRE();
		LOG(LL_NFO, "Opening session: ", (uint64_t)this);
		POST();
	}

	Session::~Session() {
		PRE();
		LOG(LL_NFO, "Closing session: ", (uint64_t)this);
		this->isBeingDeleted = true;
		this->isCapturing = false;
		LOG_CALL(LL_DBG, this->videoFrameQueue.enqueue(Encoder::Session::frameQueueItem(nullptr)));

		if (thread_video_encoder.joinable()) {
			thread_video_encoder.join();
		}

		LOG_CALL(LL_DBG, this->finishVideo());
		LOG_CALL(LL_DBG, this->finishAudio());
		LOG_CALL(LL_DBG, this->endSession());


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
		LOG_CALL(LL_DBG, avcodec_close(this->audioCodecContext));
		LOG_CALL(LL_DBG, av_free(this->audioCodecContext));
		LOG_CALL(LL_DBG, av_free(this->inputAudioFrame));
		LOG_CALL(LL_DBG, av_free(this->outputAudioFrame));
		LOG_CALL(LL_DBG, av_audio_fifo_free(this->audioSampleBuffer));
		POST();
	}

	HRESULT Session::createVideoContext(UINT width, UINT height, std::string inputPixelFormatString, UINT fps_num, UINT fps_den, std::string outputPixelFormatString, std::string vcodec, std::string preset)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		std::lock_guard<std::mutex> guard(this->mxVideoContext);
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

		RET_IF_FAILED(this->createVideoFrames(width, height, this->inputPixelFormat, width, height, this->outputPixelFormat), "Could not create video frames", E_FAIL);

		this->videoCodec = avcodec_find_encoder_by_name(vcodec.c_str());
		RET_IF_NULL(this->videoCodec, "Could not find video codec:" + vcodec, E_FAIL);

		this->videoCodecContext = avcodec_alloc_context3(this->videoCodec);
		RET_IF_NULL(this->videoCodecContext, "Could not allocate context for the video codec", E_FAIL);
		/*av_opt_set_defaults(this->videoCodecContext);
		av_opt_set_defaults(this->videoCodecContext->priv_data);*/
		//av_opt_show2(this->videoCodecContext, NULL, 0xFFFFFFFF, 0);
		//this->videoCodecContext->coder_type = 1;
		//LOG("");
		//av_opt_show2(this->videoCodecContext->priv_data, NULL, 0xFFFFFFFF, 0);
		

		//av_opt_set_from_string(this->videoCodecContext, preset.c_str(), NULL, "=", ":");
		av_dict_parse_string(&this->videoOptions, preset.c_str(), "=", "/", 0);


		this->videoCodecContext->pix_fmt = this->outputPixelFormat;
		this->videoCodecContext->width = width;
		this->videoCodecContext->height = height;
		this->videoCodecContext->time_base = av_make_q(fps_den, fps_num);
		this->videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
		
		this->thread_video_encoder = std::thread(&Session::videoEncodingThread, this);

		LOG(LL_NFO, "Video context was created successfully.");
		this->isVideoContextCreated = true;
		this->cvVideoContext.notify_all();
		POST();
		return S_OK;
	}

	HRESULT Session::createAudioContext(uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample, AVSampleFormat inputSampleFormat, uint32_t inputAlignment, uint32_t outputSampleRate, std::string outputSampleFormatString, std::string acodec, std::string preset) {
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		std::lock_guard<std::mutex> guard(this->mxAudioContext);
		LOG(LL_NFO, "Creating audio context:");
		LOG(LL_NFO, "  encoder: ", acodec);
		LOG(LL_NFO, "  options: ", preset);
		//AVCodecID audioCodecId = AV_CODEC_ID_PCM_S16LE;

		this->inputAudioSampleRate = inputSampleRate;
		this->inputAudioChannels = inputChannels;
		this->outputAudioSampleRate = outputSampleRate;
		this->outputAudioChannels = inputChannels; // FIXME

		AVSampleFormat outputSampleFormat = av_get_sample_fmt(outputSampleFormatString.c_str());
		if (outputSampleFormat == AV_SAMPLE_FMT_NONE) {
			LOG(LL_ERR, "Unknown audio sample format specified: ", outputSampleFormatString);
			POST();
			return E_FAIL;
		}
		this->outputAudioSampleFormat = outputSampleFormat;

		this->audioCodec = avcodec_find_encoder_by_name(acodec.c_str());
		RET_IF_NULL(this->audioCodec, "Could not find audio codec:" + acodec, E_FAIL);
		this->audioCodecContext = avcodec_alloc_context3(this->audioCodec);
		RET_IF_NULL(this->audioCodecContext, "Could not allocate context for the audio codec", E_FAIL);

		this->audioCodecContext->channels = inputChannels;
		this->audioCodecContext->sample_rate = outputSampleRate;
		//this->audioCodecContext->bits_per_raw_sample = bitsPerSample;
		//this->audioCodecContext->bit_rate = sampleRate * bitsPerSample * numChannels;
		this->audioCodecContext->sample_fmt = outputSampleFormat;
		this->audioCodecContext->time_base = { 1, (int)inputSampleRate}; // FIXME: What should I do with it?
		this->audioBlockAlign = inputAlignment;

		RET_IF_FAILED(this->createAudioFrames(inputChannels, inputSampleFormat, inputSampleRate, inputChannels, outputSampleFormat, outputSampleRate), "Could not create audio frames", E_FAIL);

		av_dict_parse_string(&this->audioOptions, preset.c_str(), "=", "/", 0);

		LOG(LL_NFO, "Audio context was created successfully.");
		this->isAudioContextCreated = true;
		this->cvAudioContext.notify_all();

		POST();
		return S_OK;
	}

	HRESULT Session::createFormatContext(LPCSTR filename)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		// Wait until video context is created
		LOG(LL_NFO, "Waiting for video context to be created...")
		{
			std::unique_lock<std::mutex> lk(this->mxVideoContext);
			while(!isVideoContextCreated) {
				this->cvVideoContext.wait_for(lk, std::chrono::milliseconds(100));
			}
		}
		
		// Wait until audio context is created
		LOG(LL_NFO, "Waiting for audio context to be created...")
		{
			std::unique_lock<std::mutex> lk(this->mxAudioContext);
			while(!isAudioContextCreated) {
				this->cvAudioContext.wait_for(lk, std::chrono::milliseconds(100));
			}
		}

		//std::lock_guard<std::mutex> guard(this->mxFormatContext);

		strcpy_s(this->filename, filename);
		LOG(LL_NFO, "Exporting to file: ", this->filename);

		this->oformat = av_guess_format(NULL, this->filename, NULL);
		RET_IF_NULL(this->oformat, "Could not create format", E_FAIL);
		//this->oformat->video_codec = AV_CODEC_ID_FFV1;
		//this->oformat->audio_codec = AV_CODEC_ID_PCM_S16LE;

		RET_IF_FAILED_AV(avformat_alloc_output_context2(&this->fmtContext, this->oformat, NULL, NULL), "Could not allocate format context", E_FAIL);
		RET_IF_NULL(this->fmtContext, "Could not allocate format context", E_FAIL);

		this->fmtContext->oformat = this->oformat;
		//this->fmtContext->video_codec_id = AV_CODEC_ID_FFV1;
		//this->fmtContext->audio_codec_id = AV_CODEC_ID_PCM_S16LE;

		this->videoStream = avformat_new_stream(this->fmtContext, this->videoCodec);
		RET_IF_NULL(this->videoStream, "Could not create new stream", E_FAIL);
		this->videoStream->time_base = this->videoCodecContext->time_base;
		this->videoStream->codec = this->videoCodecContext;
		this->videoStream->index = 0;

		this->audioStream = avformat_new_stream(this->fmtContext, this->audioCodec);
		RET_IF_NULL(this->audioStream, "Could not create new stream", E_FAIL);
		this->audioStream->time_base = this->audioCodecContext->time_base;
		this->audioStream->codec = this->audioCodecContext;
		this->audioStream->index = 1;

		if (this->fmtContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			this->videoCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
			this->audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		RET_IF_FAILED_AV(avcodec_open2(this->videoCodecContext, this->videoCodec, &this->videoOptions), "Could not open video codec", E_FAIL);
		RET_IF_FAILED_AV(avcodec_open2(this->audioCodecContext, this->audioCodec, &this->audioOptions), "Could not open audio codec", E_FAIL);
		RET_IF_FAILED_AV(avio_open(&this->fmtContext->pb, filename, AVIO_FLAG_WRITE), "Could not open output file", E_FAIL);
		RET_IF_NULL(this->fmtContext->pb, "Could not open output file", E_FAIL);
		RET_IF_FAILED_AV(avformat_write_header(this->fmtContext, NULL), "Could not write header", E_FAIL);
		LOG(LL_NFO, "Format context was created successfully.");
		this->isCapturing = true;
		this->isFormatContextCreated = true;
		this->cvFormatContext.notify_all();

		POST();
		return S_OK;
	}

	HRESULT Session::enqueueVideoFrame(BYTE *pData, int length) {
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		auto pVector = std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>(pData, pData + length));
		frameQueueItem item(pVector);
		this->videoFrameQueue.enqueue(item);
		POST();
		return S_OK;
	}

	void Session::videoEncodingThread() {
		PRE();
		std::lock_guard<std::mutex> lock(this->mxEncodingThread);
		try {
			frameQueueItem item = this->videoFrameQueue.dequeue();
			while (item.data != nullptr) {
				LOG(LL_NFO, "Encoding frame: ", this->videoPTS);
				REQUIRE(this->writeVideoFrame(item.data->data(), item.data->size(), this->videoPTS++), "Failed to write video frame.");
				item = this->videoFrameQueue.dequeue();
			}
		} catch (...) {
			// Do nothing
		}
		this->isEncodingThreadFinished = true;
		this->cvEncodingThreadFinished.notify_all();
		POST();
	}

	HRESULT Session::writeVideoFrame(BYTE *pData, int length, LONGLONG sampleTime) {
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}

		// Wait until format context is created
		{
			std::unique_lock<std::mutex> lk(this->mxFormatContext);
			while (!isFormatContextCreated) {
				this->cvFormatContext.wait_for(lk, std::chrono::milliseconds(100));
			}
		}


		int bufferLength = av_image_get_buffer_size(this->inputPixelFormat, this->width, this->height, 1);
		if (length != bufferLength) {
			LOG(LL_ERR, "IMFSample buffer size != av_image_get_buffer_size: ", length, " vs ", bufferLength);
			POST();
			return E_FAIL;
		}

		RET_IF_FAILED(av_image_fill_arrays(inputFrame->data, inputFrame->linesize, pData, this->inputPixelFormat, this->width, this->height, 1), "Could not fill the frame with data from the buffer", E_FAIL);

		sws_scale(pSwsContext, inputFrame->data, inputFrame->linesize, 0, this->height, outputFrame->data, outputFrame->linesize);
		//outputFrame->pts = av_rescale_q(sampleTime, this->videoCodecContext->time_base, this->videoStream->time_base);
		outputFrame->pts = sampleTime;

		std::shared_ptr<AVPacket> pPkt(new AVPacket(), av_packet_unref);

		av_init_packet(pPkt.get());
		pPkt->data = NULL;
		pPkt->size = 0;

		int got_packet;
		avcodec_encode_video2(this->videoCodecContext, pPkt.get(), outputFrame, &got_packet);
		if (got_packet != 0) {
			std::lock_guard<std::mutex> guard(this->mxWriteFrame);
			av_packet_rescale_ts(pPkt.get(), this->videoCodecContext->time_base, this->videoStream->time_base);
			pPkt->stream_index = this->videoStream->index;
			av_interleaved_write_frame(this->fmtContext, pPkt.get());
		}

		POST();
		return S_OK;
	}

	HRESULT Session::writeAudioFrame(BYTE *pData, int length, LONGLONG sampleTime)
	{
		PRE();
		if (this->isBeingDeleted) {
			POST();
			return E_FAIL;
		}
		// Wait until format context is created
		{
			std::unique_lock<std::mutex> lk(this->mxFormatContext);
			while (!isFormatContextCreated) {
				this->cvFormatContext.wait_for(lk, std::chrono::milliseconds(100));
			}
		}
		
		int numSamples = length / av_samples_get_buffer_size(NULL, this->inputAudioFrame->channels, 1, (AVSampleFormat)this->inputAudioFrame->format, this->audioBlockAlign);

		this->inputAudioFrame->nb_samples = numSamples;
		//this->inputAudioFrame->format = this->audioCodecContext->sample_fmt;
		//this->outputAudioFrame->nb_samples = this->audioCodecContext->frame_size;

		int numOutSamples = av_rescale_rnd(swr_get_delay(this->pSwrContext, this->inputAudioSampleRate) + numSamples, this->outputAudioSampleRate, this->inputAudioSampleRate, AV_ROUND_UP);
		this->outputAudioFrame->nb_samples = numOutSamples;

		avcodec_fill_audio_frame(this->inputAudioFrame, this->inputAudioFrame->channels, (AVSampleFormat)this->inputAudioFrame->format, pData, length, this->audioBlockAlign);

		RET_IF_FAILED_AV(swr_convert_frame(this->pSwrContext, this->outputAudioFrame, this->inputAudioFrame), "Failed to convert audio frame", E_FAIL);

		av_audio_fifo_write(this->audioSampleBuffer, (void**)this->outputAudioFrame->data, this->outputAudioFrame->nb_samples);
		if (av_audio_fifo_size(this->audioSampleBuffer) < this->audioCodecContext->frame_size) {
			POST();
			return S_OK;
		}

		int bufferSize = av_samples_get_buffer_size(NULL, this->outputAudioChannels, this->audioCodecContext->frame_size, this->outputAudioSampleFormat, 0);
		uint8_t* localBuffer = new uint8_t[bufferSize];
		AVFrame* localFrame = av_frame_alloc();
		localFrame->channel_layout = AV_CH_LAYOUT_STEREO;
		localFrame->format = this->outputAudioSampleFormat;
		localFrame->nb_samples = this->audioCodecContext->frame_size;
		avcodec_fill_audio_frame(localFrame, this->outputAudioChannels, this->outputAudioSampleFormat, localBuffer, bufferSize, 0);
		av_audio_fifo_read(this->audioSampleBuffer, (void**)localFrame->data, this->audioCodecContext->frame_size);



		//audioFrame->pts = sampleTime;
		//audioFrame->pts = av_rescale_q(sampleTime, MF_TIME_BASE, this->audioStream->time_base);
		localFrame->pts = AV_NOPTS_VALUE;

		std::shared_ptr<AVPacket> pPkt(new AVPacket(), av_packet_unref);

		av_init_packet(pPkt.get());
		pPkt->data = NULL;
		pPkt->size = 0;

		int got_packet;
		avcodec_encode_audio2(this->audioCodecContext, pPkt.get(), localFrame, &got_packet);

		delete[] localBuffer;
		av_frame_free(&localFrame);
		if (got_packet != 0) {
			std::lock_guard<std::mutex> guard(this->mxWriteFrame);
			av_packet_rescale_ts(pPkt.get(), this->audioCodecContext->time_base, this->audioStream->time_base);
			//pPkt->dts = audioFrame->pts;
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
		if (this->isVideoFinished || !this->isVideoContextCreated || this->isBeingDeleted) {
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

		if (thread_video_encoder.joinable()) {
			thread_video_encoder.join();
		}

		// Write delayed frames
		{
			AVPacket pkt;

			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;

			int got_packet;
			avcodec_encode_video2(this->videoCodecContext, &pkt, NULL, &got_packet);
			while (got_packet != 0) {
				std::lock_guard<std::mutex> guard(this->mxWriteFrame);
				av_packet_rescale_ts(&pkt, this->videoCodecContext->time_base, this->videoStream->time_base);
				//pkt.dts = outputFrame->pts;
				pkt.stream_index = this->videoStream->index;
				av_interleaved_write_frame(this->fmtContext, &pkt);
				avcodec_encode_video2(this->videoCodecContext, &pkt, NULL, &got_packet);
			}

			av_free_packet(&pkt);
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
		if (this->isAudioFinished || !this->isAudioContextCreated || this->isBeingDeleted) {
			POST();
			return S_OK;
		}

		// Write delated frames
		{

			LOG(LL_WRN, "FIXME: Audio samples discarded:", av_audio_fifo_size(this->audioSampleBuffer));

			AVPacket pkt;

			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;

			int got_packet;
			avcodec_encode_audio2(this->audioCodecContext, &pkt, NULL, &got_packet);
			while (got_packet != 0) {
				std::lock_guard<std::mutex> guard(this->mxWriteFrame);
				//av_packet_rescale_ts(&pkt, this->audioCodecContext->time_base, this->audioStream->time_base);
				pkt.stream_index = this->audioStream->index;
				av_interleaved_write_frame(this->fmtContext, &pkt);
				avcodec_encode_audio2(this->audioCodecContext, &pkt, NULL, &got_packet);
			}

			av_free_packet(&pkt);
			av_packet_unref(&pkt);
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
		LOG_IF_FAILED_AV(avio_close(this->fmtContext->pb), "Could not close the output file.");
		LOG_IF_FAILED_AV(avcodec_close(this->videoCodecContext), "Could not close the video codec.");
		LOG_IF_FAILED_AV(avcodec_close(this->audioCodecContext), "Could not close the audio codec.");
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
		av_image_alloc(outputFrame->data, outputFrame->linesize, dstWidth, dstHeight, dstFmt, 1);

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

		this->audioSampleBuffer = av_audio_fifo_alloc(outputSampleFmt, outputChannels, 4096);


		this->pSwrContext = swr_alloc_set_opts(NULL,
			AV_CH_LAYOUT_STEREO,
			outputSampleFmt,
			outputSampleRate,
			AV_CH_LAYOUT_STEREO,
			inputSampleFmt,
			inputSampleRate,
			AV_LOG_TRACE, NULL);

		RET_IF_NULL(this->pSwrContext, "Could not allocate audio resampling context", E_FAIL);
		RET_IF_FAILED_AV(swr_init(pSwrContext), "Could not initialize audio resampling context", E_FAIL);

		POST();
		return S_OK;
	}
}
