#pragma once

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")

#include <Windows.h>
#include <mfidl.h>
#include <mutex>
#include <future>
#include <vector>
#include <valarray>
#include "SafeQueue.h"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl.h>

using namespace Microsoft::WRL;

extern "C" {
#include <libavutil\audio_fifo.h>
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libavutil\imgutils.h>
#include <libswresample\swresample.h>
#include <libswscale\swscale.h>
}

namespace Encoder {
	class Session {
	public:
		AVOutputFormat *oformat = NULL;
		AVFormatContext *fmtContext = NULL;
		AVDictionary *fmtOptions = NULL;
		
		AVCodec *videoCodec = NULL;
		AVCodecContext *videoCodecContext = NULL;
		AVFrame *inputFrame = NULL;
		AVFrame *outputFrame = NULL;
		AVStream *videoStream = NULL;
		SwsContext *pSwsContext = NULL;
		AVDictionary *videoOptions = NULL;
		uint64_t videoPTS = 0;
		uint64_t motionBlurPTS = 0;

		AVCodec *audioCodec = NULL;
		AVCodecContext *audioCodecContext = NULL;
		AVFrame *inputAudioFrame = NULL;
		AVFrame *outputAudioFrame = NULL;
		AVStream *audioStream = NULL;
		SwrContext* pSwrContext = NULL;
		AVDictionary *audioOptions = NULL;
		AVAudioFifo *audioSampleBuffer = NULL;
		uint64_t audioPTS = 0;


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
			frameQueueItem():
				frame_mapping({ 0 }),
				frame(nullptr)
			{
				
			}

			frameQueueItem(ComPtr<ID3D11Texture2D> frame, D3D11_MAPPED_SUBRESOURCE *frame_mapping) :
				frame(frame)
			{
				if (frame_mapping) {
					this->frame_mapping = *frame_mapping;
				}
			}

			//std::shared_ptr<std::valarray<uint8_t>> data;
			D3D11_MAPPED_SUBRESOURCE frame_mapping;
			ComPtr<ID3D11Texture2D> frame;
		};

		struct exr_queue_item {
			exr_queue_item() :
				cRGB(nullptr),
				pRGBData(nullptr),
				cDepth(nullptr),
				pDepthData(nullptr),
				isEndOfStream(true)
			{ }

			exr_queue_item(ComPtr<ID3D11Texture2D> cRGB, void *pRGBData, ComPtr<ID3D11Texture2D> cDepth, void *pDepthData, ComPtr<ID3D11Texture2D> cStencil, D3D11_MAPPED_SUBRESOURCE mStencilData) :
				cRGB(cRGB),
				pRGBData(pRGBData),
				cDepth(cDepth),
				pDepthData(pDepthData),
				cStencil(cStencil),
				mStencilData(mStencilData)
			{ }

			bool isEndOfStream = false;

			ComPtr<ID3D11Texture2D> cRGB;
			void* pRGBData;
			ComPtr<ID3D11Texture2D> cDepth;
			void* pDepthData;
			ComPtr<ID3D11Texture2D> cStencil;
			D3D11_MAPPED_SUBRESOURCE mStencilData;
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

		bool isEXREncodingThreadFinished = false;
		std::condition_variable cvEXREncodingThreadFinished;
		std::mutex mxEXREncodingThread;
		std::thread thread_exr_encoder;

		std::mutex mxFinish;
		std::mutex mxWriteFrame;

		UINT width;
		UINT height;
		UINT framerate;
		//float audioSampleRateMultiplier;
		uint32_t motionBlurSamples;
		UINT audioBlockAlign;
		AVPixelFormat outputPixelFormat;
		AVPixelFormat inputPixelFormat;
		AVSampleFormat inputAudioSampleFormat;
		AVSampleFormat outputAudioSampleFormat;
		UINT inputAudioSampleRate;
		UINT inputAudioChannels;
		UINT outputAudioSampleRate;
		UINT outputAudioChannels;
		std::string filename;
		std::string exrOutputPath;
		uint64_t exrPTS=0;
		float shutterPosition;
		//LPWSTR *outputDir;
		//LPWSTR *outputFile;
		//FILE *file;

		Session();

		~Session();

		HRESULT createContext(
			std::string format,
			std::string filename,
			std::string exrOutputPath,
			std::string fmtOptions,
			uint64_t width,
			uint64_t height,
			std::string inputPixelFmt,
			uint32_t fps_num,
			uint32_t fps_den,
			uint8_t motionBlurSamples,
			float shutterPosition,
			std::string outputPixelFmt,
			std::string vcodec,
			std::string voptions,
			uint32_t inputChannels,
			uint32_t inputSampleRate,
			uint32_t inputBitsPerSample,
			std::string inputSampleFmt,
			uint32_t inputAlign,
			std::string outputSampleFmt,
			std::string acodec,
			std::string aoptions
			);

		HRESULT enqueueVideoFrame(ComPtr<ID3D11DeviceContext> p_ctx, ComPtr<ID3D11Texture2D> p_frame);
		HRESULT enqueueEXRImage(ComPtr<ID3D11DeviceContext> pDeviceContext, ComPtr<ID3D11Texture2D> cRGB, ComPtr<ID3D11Texture2D> cDepth, ComPtr<ID3D11Texture2D> cStencil);

		void videoEncodingThread();
		void exrEncodingThread();

		HRESULT writeVideoFrame(BYTE *pData, size_t length, LONGLONG sampleTime);
		HRESULT writeAudioFrame(BYTE *pData, size_t length, LONGLONG sampleTime);

		HRESULT finishVideo();
		HRESULT finishAudio();

		HRESULT endSession();

	private:
		HRESULT createVideoContext(UINT width, UINT height, std::string inputPixelFormatString, UINT fps_num, UINT fps_den, uint8_t motionBlurSamples, float shutterPosition, std::string outputPixelFormatString, std::string vcodec, std::string preset);
		HRESULT createAudioContext(uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample, std::string inputSampleFormat, uint32_t inputAlignment, std::string outputSampleFormatString, std::string acodec, std::string preset);
		HRESULT createFormatContext(std::string format, std::string filename, std::string exrOutputPath, std::string fmtOptions);
		HRESULT createVideoFrames(uint32_t srcWidth, uint32_t srcHeight, AVPixelFormat srcFmt, uint32_t dstWidth, uint32_t dstHeight, AVPixelFormat dstFmt);
		HRESULT createAudioFrames(uint32_t inputChannels, AVSampleFormat inputSampleFmt, uint32_t inputSampleRate, uint32_t outputChannels, AVSampleFormat outputSampleFmt, uint32_t outputSampleRate);
	};
}