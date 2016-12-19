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
#include "SafeQueue.h"
extern "C" {
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libavutil\imgutils.h>
#include <libswresample\swresample.h>
}

//template<typename T>
//using std::shared_ptr = std::shared_ptr<T, std::function<void(T*)>>;

namespace Encoder {
	class Session {
	public:
		AVOutputFormat *oformat;
		AVFormatContext *fmtContext;
		
		AVCodec *videoCodec = NULL;
		AVCodecContext *videoCodecContext = NULL;
		AVFrame *inputFrame = NULL;
		AVFrame *outputFrame = NULL;
		AVStream *videoStream = NULL;
		SwsContext *pSwsContext = NULL;
		AVDictionary *videoOptions = NULL;
		uint64_t videoPTS = 1;

		AVCodec *audioCodec = NULL;
		AVCodecContext *audioCodecContext = NULL;
		AVFrame *inputAudioFrame = NULL;
		AVFrame *outputAudioFrame = NULL;
		AVStream *audioStream = NULL;
		SwrContext* pSwrContext = NULL;
		AVDictionary *audioOptions = NULL;
		AVAudioFifo *audioSampleBuffer = NULL;


		bool isVideoFinished = false;
		bool isAudioFinished = false;
		bool isSessionFinished = false;
		bool isCapturing = false;
		bool isBeingDeleted = false;

		std::mutex mxVideoContext;
		std::mutex mxAudioContext;
		std::mutex mxFormatContext;
		std::condition_variable cvVideoContext;
		std::condition_variable cvAudioContext;
		std::condition_variable cvFormatContext;

		std::mutex mxEndSession;
		std::condition_variable cvEndSession;

		struct frameQueueItem {
			frameQueueItem():
				data(nullptr)
			{
				
			}
			frameQueueItem(std::shared_ptr<std::vector<uint8_t>> bytes) :
				data(bytes)
			{}

			std::shared_ptr<std::vector<uint8_t>> data;
		};

		SafeQueue<frameQueueItem> videoFrameQueue;

		bool isVideoContextCreated = false;
		bool isAudioContextCreated = false;
		bool isFormatContextCreated = false;
		bool isEncodingThreadFinished = false;
		std::condition_variable cvEncodingThreadFinished;
		std::mutex mxEncodingThread;
		std::thread thread_video_encoder;
		std::vector<uint16_t> motionBlurAccBuffer;
		std::vector<uint8_t> motionBlurDestBuffer;

		//std::condition_variable cvFormatContext;

		std::mutex mxFinish;
		std::mutex mxWriteFrame;

		UINT width;
		UINT height;
		UINT framerate;
		UINT audioBlockAlign;
		AVPixelFormat outputPixelFormat;
		AVPixelFormat inputPixelFormat;
		AVSampleFormat inputAudioSampleFormat;
		AVSampleFormat outputAudioSampleFormat;
		UINT inputAudioSampleRate;
		UINT inputAudioChannels;
		UINT outputAudioSampleRate;
		UINT outputAudioChannels;
		char filename[MAX_PATH];
		//LPWSTR *outputDir;
		//LPWSTR *outputFile;
		//FILE *file;

		Session();

		~Session();

		HRESULT createVideoContext(UINT width, UINT height, std::string inputPixelFormatString, UINT fps_num, UINT fps_den, std::string outputPixelFormatString, std::string vcodec, std::string preset);
		HRESULT createAudioContext(uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample, AVSampleFormat inputSampleFormat, uint32_t inputAlignment, uint32_t outputSampleRate, std::string outputSampleFormatString, std::string acodec, std::string preset);
		HRESULT createFormatContext(LPCSTR filename);

		HRESULT enqueueVideoFrame(BYTE * pData, int length);

		void videoEncodingThread();

		HRESULT writeVideoFrame(BYTE *pData, int length, LONGLONG sampleTime);
		HRESULT writeAudioFrame(BYTE *pData, int length, LONGLONG sampleTime);

		HRESULT finishVideo();
		HRESULT finishAudio();

		HRESULT endSession();

	private:
		HRESULT createVideoFrames(uint32_t srcWidth, uint32_t srcHeight, AVPixelFormat srcFmt, uint32_t dstWidth, uint32_t dstHeight, AVPixelFormat dstFmt);
		HRESULT createAudioFrames(uint32_t inputChannels, AVSampleFormat inputSampleFmt, uint32_t inputSampleRate, uint32_t outputChannels, AVSampleFormat outputSampleFmt, uint32_t outputSampleRate);
	};
}