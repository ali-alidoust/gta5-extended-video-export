#include "encoder.h"
#include <sstream>
#include <unordered_map>
#include "logger.h"


namespace Encoder {
	
	const AVRational MF_TIME_BASE = { 1, 10000000 };

	Session::Session() {
		LOG("Creating session...");
	}

	Session::~Session() {
		LOG("Deleting session...");
	}

	HRESULT Session::createVideoContext(UINT width, UINT height, AVPixelFormat inputFramePixelFormat, UINT fps_num, UINT fps_den) {
		this->inputFramePixelFormat = inputFramePixelFormat;

		AVCodecID videoCodecId = AV_CODEC_ID_FFV1;
		this->pixelFormat = AV_PIX_FMT_YUV420P;

		this->videoCodec = avcodec_find_encoder(videoCodecId);
		RET_IF_NULL(this->videoCodec, "Could not create codec", E_FAIL);

		
		this->width = width;
		this->height = height;
		this->videoCodecContext = avcodec_alloc_context3(this->videoCodec);
		RET_IF_NULL(this->videoCodecContext, "Could not allocate context for the codec", E_FAIL);

		this->videoCodecContext->slices = 16;
		this->videoCodecContext->codec = this->videoCodec;
		this->videoCodecContext->codec_id = videoCodecId;

		this->videoCodecContext->pix_fmt = pixelFormat;
		this->videoCodecContext->width = this->width;
		this->videoCodecContext->height = this->height;
		this->videoCodecContext->time_base = av_make_q(fps_den, fps_num);
		this->videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

		this->videoCodecContext->gop_size = 1;

		videoFrame = av_frame_alloc();
		RET_IF_NULL(videoFrame, "Could not allocate video frame", E_FAIL);
		videoFrame->format = this->videoCodecContext->pix_fmt;
		videoFrame->width = width;
		videoFrame->height = height;
		return S_OK;
	}

	HRESULT Session::createAudioContext(UINT numChannels, UINT sampleRate, UINT bitsPerSample, AVSampleFormat sampleFormat, UINT align)
	{
		AVCodecID audioCodecId = AV_CODEC_ID_PCM_S16LE;
		this->audioCodec = avcodec_find_encoder(audioCodecId);
		this->audioCodecContext = avcodec_alloc_context3(this->audioCodec);

		this->audioCodecContext->channels = numChannels;
		this->audioCodecContext->sample_rate = sampleRate;
		this->audioCodecContext->bits_per_raw_sample = bitsPerSample;
		this->audioCodecContext->bit_rate = sampleRate * bitsPerSample * numChannels;
		this->audioCodecContext->sample_fmt = sampleFormat;
		this->audioCodecContext->time_base = { 1, 48000 }; // Probably unused.
		this->audioBlockAlign = align;

		this->audioFrame = av_frame_alloc();
		RET_IF_NULL(audioFrame, "Could not allocate audio frame", E_FAIL);
		audioFrame->format = sampleFormat;
		audioFrame->sample_rate = sampleRate;
		audioFrame->channels = numChannels;

		return S_OK;
	}

	HRESULT Session::createFormatContext(LPCSTR filename)
	{
		strcpy_s(this->filename, filename);
		LOG("Exporting to file: ", this->filename);

		this->oformat = av_guess_format(NULL, this->filename, NULL);
		RET_IF_NULL(this->oformat, "Could not create format", E_FAIL);
		this->oformat->video_codec = AV_CODEC_ID_FFV1;
		this->oformat->audio_codec = AV_CODEC_ID_PCM_S16LE;

		RET_IF_FAILED_AV(avformat_alloc_output_context2(&fmtContext, this->oformat, NULL, NULL), "Could not allocate format context", E_FAIL);
		RET_IF_NULL(this->fmtContext, "Could not allocate format context", E_FAIL);

		this->fmtContext->oformat = this->oformat;
		this->fmtContext->video_codec_id = AV_CODEC_ID_FFV1;
		this->fmtContext->audio_codec_id = AV_CODEC_ID_PCM_S16LE;

		this->videoStream = avformat_new_stream(this->fmtContext, this->videoCodec);
		RET_IF_NULL(this->videoStream, "Could not create new stream", E_FAIL);
		this->videoStream->time_base = this->videoCodecContext->time_base;
		this->videoStream->codec = this->videoCodecContext;
		this->videoStream->index = 0;

		this->audioStream = avformat_new_stream(this->fmtContext, this->audioCodec);
		RET_IF_NULL(this->audioStream, "Could not create new stream", E_FAIL);
		this->audioStream->time_base = av_make_q(1, this->audioCodecContext->sample_rate);
		this->audioStream->codec = this->audioCodecContext;
		this->audioStream->index = 1;

		if (this->fmtContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			this->videoCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
			this->audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		RET_IF_FAILED_AV(avcodec_open2(this->videoCodecContext, this->videoCodec, NULL), "Could not open codec", E_FAIL);
		RET_IF_FAILED_AV(avcodec_open2(this->audioCodecContext, this->audioCodec, NULL), "Could not open codec", E_FAIL);
		RET_IF_FAILED_AV(avio_open(&this->fmtContext->pb, filename, AVIO_FLAG_WRITE), "Could not open output file", E_FAIL);
		RET_IF_NULL(this->fmtContext->pb, "Could not open output file", E_FAIL);
		RET_IF_FAILED_AV(avformat_write_header(this->fmtContext, NULL), "Could not write header", E_FAIL);

		return S_OK;
	}

	HRESULT Session::writeVideoFrame(BYTE *pData, int length, LONGLONG sampleTime) {
		int bufferLength = av_image_get_buffer_size(this->inputFramePixelFormat, this->width, this->height, 1);
		if (length != bufferLength) {
			LOG("IMFSample buffer size != av_image_get_buffer_size: ", length, " vs ", bufferLength);
			return E_FAIL;
		}

		BYTE *pDataYUV420P = new BYTE[length];
		switch (this->inputFramePixelFormat) {
		case AV_PIX_FMT_YUV420P:
			memcpy_s(pDataYUV420P, length, pData, length);
			break;

		case AV_PIX_FMT_NV12:
			this->convertNV12toYUV420P(pData, pDataYUV420P, this->width, this->height);
			break;
			
		default:
			LOG("Could not recognize pixel format.");
			delete[] pDataYUV420P;
			return E_FAIL;
		}

		RET_IF_FAILED(av_image_fill_arrays(videoFrame->data, videoFrame->linesize, pDataYUV420P, pixelFormat, this->width, this->height, 1), "Could not fill the frame with data from the buffer", E_FAIL);
		videoFrame->pts = av_rescale_q(sampleTime, MF_TIME_BASE, this->videoStream->time_base);
		
		AVPacket pkt;

		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		int got_packet;
		avcodec_encode_video2(this->videoCodecContext, &pkt, videoFrame, &got_packet);
		if (got_packet != 0) {
			std::lock_guard<std::mutex> guard(this->writeFrameMutex);
			pkt.stream_index = this->videoStream->index;
			av_interleaved_write_frame(this->fmtContext, &pkt);
		}

		delete[] pDataYUV420P;

		av_free_packet(&pkt);
		av_packet_unref(&pkt);

		return S_OK;
	}

	HRESULT Session::writeAudioFrame(BYTE *pData, int length, LONGLONG sampleTime)
	{
		int numSamples = length / av_samples_get_buffer_size(NULL, this->audioCodecContext->channels, 1, this->audioCodecContext->sample_fmt, this->audioBlockAlign);

		this->audioFrame->nb_samples = numSamples;
		this->audioFrame->format = this->audioCodecContext->sample_fmt;
		avcodec_fill_audio_frame(this->audioFrame, this->audioCodecContext->channels, this->audioCodecContext->sample_fmt, pData, length, this->audioBlockAlign);

		audioFrame->pts = AV_NOPTS_VALUE;

		AVPacket pkt;

		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		int got_packet;
		avcodec_encode_audio2(this->audioCodecContext, &pkt, this->audioFrame, &got_packet);
		if (got_packet != 0) {
			std::lock_guard<std::mutex> guard(this->writeFrameMutex);
			pkt.stream_index = this->audioStream->index;
			av_interleaved_write_frame(this->fmtContext, &pkt);
		}

		av_free_packet(&pkt);
		av_packet_unref(&pkt);

		return S_OK;
	}

	HRESULT Session::finishVideo()
	{
		std::lock_guard<std::mutex> guard(this->endMutex);
		this->isVideoFinished = true;
		this->endSession();
		return S_OK;
	}

	HRESULT Session::finishAudio()
	{
		std::lock_guard<std::mutex> guard(this->endMutex);
		this->isAudioFinished = true;
		this->endSession();
		return S_OK;
	}

	HRESULT Session::endSession() {
		if (this->isVideoFinished && this->isAudioFinished) {
			LOG("Ending session...");

			LOG("Closing files...");
				LOG_IF_FAILED_AV(av_write_trailer(this->fmtContext), "Could not finalize the output file.");
			LOG_IF_FAILED_AV(avio_close(this->fmtContext->pb), "Could not close the output file.");
			LOG_IF_FAILED_AV(avcodec_close(this->videoCodecContext), "Could not close the video codec.");
			LOG_IF_FAILED_AV(avcodec_close(this->audioCodecContext), "Could not close the audio codec.");
			av_free(this->videoCodecContext);
			av_free(this->audioCodecContext);
			this->isSessionFinished = true;
			LOG("Done.");
		}
		return S_OK;
	}
}
