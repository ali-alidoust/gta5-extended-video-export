#include "encoder.h"
#include <sstream>
#include <unordered_map>
#include "logger.h"


namespace Encoder {
	

	std::unordered_map<IMFTransform*, Session*> sessions;



	HRESULT createSession(IMFTransform* pTransform) {
		if (sessions[pTransform] != NULL) {
			return E_ABORT;
		}
		Session* pSession = new Session();
		sessions[pTransform] = pSession;
		return S_OK;
	}

	HRESULT getSession(IMFTransform* pTransform, Session** ppEncoder) {
		if (sessions[pTransform] == NULL) {
			*ppEncoder = NULL;
			return E_FAIL;
		}
		
		*ppEncoder = sessions[pTransform];
		return S_OK;
	}

	HRESULT deleteSession(IMFTransform* pTransform) {
		if (sessions[pTransform] == NULL) {
			return E_ABORT;
		}

		sessions[pTransform] = NULL;
		return S_OK;
	}

	BOOL hasSession(IMFTransform* pTransform) {
		return (sessions[pTransform] != NULL);
	}

	inline Session::Session() {
		LOG("Creating session...");
	}

	inline Session::~Session() {
		this->endSession();
	}

	HRESULT Session::createContext(LPCSTR filename, UINT width, UINT height, AVPixelFormat inputFramePixelFormat, UINT fps_num, UINT fps_den) {
		LOG("Exporting to file: ", filename);

		this->inputFramePixelFormat = inputFramePixelFormat;

		AVCodecID codecId = AV_CODEC_ID_FFV1;
		this->pixelFormat = AV_PIX_FMT_YUV420P;

		this->codec = avcodec_find_encoder(codecId);
		RET_IF_NULL(this->codec, "Could not create codec", E_FAIL);

		this->oformat = av_guess_format(NULL, filename, NULL);
		RET_IF_NULL(this->oformat, "Could not create format", E_FAIL);
		this->oformat->video_codec = codecId;
		this->width = width;
		this->height = height;
		this->codecContext = avcodec_alloc_context3(this->codec);
		RET_IF_NULL(this->codecContext, "Could not allocate context for the codec", E_FAIL);

		this->codecContext->slices = 16;
		this->codecContext->codec = this->codec;
		this->codecContext->codec_id = codecId;

		this->codecContext->pix_fmt = pixelFormat;
		this->codecContext->width = this->width;
		this->codecContext->height = this->height;
		this->codecContext->time_base.num = fps_den;
		this->codecContext->time_base.den = fps_num;
		this->codecContext->codec_type = AVMEDIA_TYPE_VIDEO;

		this->codecContext->gop_size = 1;
		

		RET_IF_FAILED_AV(avformat_alloc_output_context2(&fmtContext, this->oformat, NULL, NULL), "Could not allocate format context", E_FAIL);
		RET_IF_NULL(this->fmtContext, "Could not allocate format context", E_FAIL);

		this->fmtContext->oformat = this->oformat;
		this->fmtContext->video_codec_id = codecId;

		this->stream = avformat_new_stream(this->fmtContext, this->codec);
		RET_IF_NULL(this->stream, "Could not create new stream", E_FAIL);
		this->stream->time_base = this->codecContext->time_base;
		//RET_IF_FAILED_AV(avcodec_parameters_from_context(this->stream->codecpar, this->codecContext), "Could not convert AVCodecContext to AVParameters", E_FAIL);
		this->stream->codec = this->codecContext;

		if (this->fmtContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			this->codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		//av_opt_set_int(this->codecContext->priv_data, "coder", 1, 0);
		//av_opt_set_int(this->codecContext->priv_data, "context", 1, 0);
		//av_opt_set_int(this->codecContext->priv_data, "slicecrc", 1, 0);
		////av_opt_set_int(this->codecContext->priv_data, "slicecrc", 1, 0);
		//av_opt_set_(this->codecContext->priv_data, "pix_fmt", pixelFormat, 0);
		//av_opt_set_int(this->codecContext->priv_data, "fmt", pixelFormat, 0);

		RET_IF_FAILED_AV(avcodec_open2(this->codecContext, this->codec, NULL), "Could not open codec", E_FAIL);
		RET_IF_FAILED_AV(avio_open(&this->fmtContext->pb, filename, AVIO_FLAG_WRITE), "Could not open output file", E_FAIL);
		RET_IF_NULL(this->fmtContext->pb, "Could not open output file", E_FAIL);
		RET_IF_FAILED_AV(avformat_write_header(this->fmtContext, NULL), "Could not write header", E_FAIL);

		frame = av_frame_alloc();
		RET_IF_NULL(frame, "Could not allocate frame", E_FAIL);
		frame->format = this->codecContext->pix_fmt;
		frame->width = width;
		frame->height = height;
		return S_OK;
	}

	HRESULT Session::writeFrame(BYTE *pData, int length) {

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


		
		
		RET_IF_FAILED(av_image_fill_arrays(frame->data, frame->linesize, pDataYUV420P, pixelFormat, this->width, this->height, 1), "Could not fill the frame with data from the buffer", E_FAIL);
		frame->pts = av_rescale_q(this->pts++, this->codecContext->time_base, this->stream->time_base);
		
		AVPacket pkt;

		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		//RET_IF_FAILED_AV(avcodec_send_frame(this->codecContext, frame), "Could not send the frame to the encoder", E_FAIL);
		int got_packet;
		avcodec_encode_video2(this->codecContext, &pkt, frame, &got_packet);
		if (got_packet != 0) {
			av_interleaved_write_frame(this->fmtContext, &pkt);
		}

		delete[] pDataYUV420P;
		/*if (SUCCEEDED(avcodec_receive_packet(this->codecContext, &pkt))) {
			RET_IF_FAILED_AV(av_interleaved_write_frame(this->fmtContext, &pkt), "Could not write the received packet.", E_FAIL);
		}*/

		av_free_packet(&pkt);
		//av_packet_unref(&pkt);

		return S_OK;
	}

	HRESULT Session::endSession() {
		LOG("Ending session...");
		
		LOG("Closing files...")
		LOG_IF_FAILED_AV(av_write_trailer(this->fmtContext), "Could not finalize the output file.");
		LOG_IF_FAILED_AV(avio_close(this->fmtContext->pb), "Could not close the output file.");
		LOG_IF_FAILED_AV(avcodec_close(this->codecContext), "Could not close the codec.");
		av_free(this->codecContext);
		LOG("Done.")
		return S_OK;
	}
}
