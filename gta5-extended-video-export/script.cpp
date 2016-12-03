// script.cpp : Defines the exported functions for the DLL application.
//

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "avcodec.lib")

extern "C" {
#include "libavcodec\avcodec.h"
}

#include "inc\main.h"
#include "custom-hooks.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include "script.h"
#include <string>
#include <regex>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include "MFUtility.h"
#include "encoder.h"
#include "logger.h"
#include "config.h"
#include <wrl.h>

#include "..\DirectXTex\DirectXTex\DirectXTex.h"

using namespace Microsoft::WRL;


namespace {
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessInput(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessMessage(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_AddStream(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_SetInputMediaType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_WriteSample(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_Finalize(new PLH::VFuncDetour);

	std::shared_ptr<PLH::IATHook> hkCoCreateInstance(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateSinkWriterFromURL(new PLH::IATHook);	
	Encoder::Session* session;
	DirectX::ScratchImage latestImage;
	std::mutex mxLatestImage;
}

static HRESULT Hook_CoCreateInstance(
	REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv
	);

static HRESULT Hook_MFCreateSinkWriterFromURL(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	);

static HRESULT IMFSinkWriter_SetInputMediaType(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters
	);

static HRESULT Hook_IMFTransform_ProcessMessage(
	IMFTransform     *pThis,
	MFT_MESSAGE_TYPE eMessage,
	ULONG_PTR        ulParam
	);

static HRESULT Hook_IMFSinkWriter_AddStream(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	);

static HRESULT Hook_IMFSinkWriter_WriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample
	);

static HRESULT Hook_IMFSinkWriter_Finalize(
	IMFSinkWriter *pThis
	);

static HRESULT Hook_IMFTransform_ProcessInput(
	IMFTransform	*pThis,
	DWORD			dwInputStreamID,
	IMFSample		*pSample,
	DWORD			dwFlags
	);


typedef HRESULT(*tMFCreateSinkWriterFromURL)(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	);

typedef HRESULT(*tIMFTransform_ProcessInput)(
	IMFTransform	*pThis,
	DWORD     dwInputStreamID,
	IMFSample *pSample,
	DWORD     dwFlags
	);

typedef HRESULT(*tIMFTransform_ProcessMessage)(
	IMFTransform     *pThis,
	MFT_MESSAGE_TYPE eMessage,
	ULONG_PTR        ulParam
	);

typedef HRESULT(*tCoCreateInstance)(
	REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv
	);

typedef HRESULT(*tIMFSinkWriter_AddStream)(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	);

typedef HRESULT (*tIMFSinkWriter_SetInputMediaType)(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters
	);

typedef HRESULT (*tIMFSinkWriter_WriteSample)(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample
	);

typedef HRESULT (*tIMFSinkWriter_Finalize)(
	IMFSinkWriter *pThis
	);

tCoCreateInstance oCoCreateInstance;
tMFCreateSinkWriterFromURL oMFCreateSinkWriterFromURL;
tIMFTransform_ProcessInput oIMFTransform_ProcessInput;
tIMFTransform_ProcessMessage oIMFTransform_ProcessMessage;
tIMFSinkWriter_SetInputMediaType oIMFSinkWriter_SetInputMediaType;
tIMFSinkWriter_AddStream oIMFSinkWriter_AddStream;
tIMFSinkWriter_WriteSample oIMFSinkWriter_WriteSample;
tIMFSinkWriter_Finalize oIMFSinkWriter_Finalize;

void avlog_callback(void *ptr, int level, const char* fmt, va_list vargs) {
	static char msg[8192];
	vsnprintf_s(msg, sizeof(msg), fmt, vargs);
	Logger::instance().write(Logger::instance().getTimestamp(), "AVCODEC: ");
	if (ptr)
	{
		AVClass *avc = *(AVClass**)ptr;
		Logger::instance().write("(");
		Logger::instance().write(avc->item_name(ptr));
		Logger::instance().write("): ");
	}
	Logger::instance().write(msg);
}

void onPresent(IDXGISwapChain *swapChain) {
	if ((session != NULL) && session->isCapturing) {
		try {
			std::lock_guard<std::mutex> lock_guard(mxLatestImage);
			ComPtr<ID3D11Device> pDevice;
			ComPtr<ID3D11DeviceContext> pDeviceContext;
			ComPtr<ID3D11Texture2D> texture;
			REQUIRE(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)texture.GetAddressOf()), "Failed to get the texture buffer", std::exception());
			REQUIRE(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)pDevice.GetAddressOf()), "Failed to get the D3D11 device", std::exception());
			pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
			NOT_NULL(pDeviceContext.Get(), "Failed to get D3D11 device context", std::exception());
			REQUIRE(DirectX::CaptureTexture(pDevice.Get(), pDeviceContext.Get(), texture.Get(), latestImage), "Failed to capture the texture", std::exception());
		} catch (std::exception& ex) {
			latestImage.Release();
		}
	}
}


void ScriptMain() {
	LOG("Starting script...");
	if (Config::instance().isLosslessExportEnabled()) {
		LOG_IF_FAILED(hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL, &oMFCreateSinkWriterFromURL, hkMFCreateSinkWriterFromURL), "Failed to hook MFCreateSinkWriterFromURL in mfreadwrite.dll");
		LOG_IF_FAILED(hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance, hkCoCreateInstance), "Failed to hook CoCreateInstance in ole32.dll");
		
		LOG_CALL(av_register_all());
		LOG_CALL(avcodec_register_all());
		LOG_CALL(av_log_set_level(AV_LOG_TRACE));
		LOG_CALL(av_log_set_callback(&avlog_callback));
	} else {
		LOG("Lossless export is disabled in the config file");
	}

	LOG("Starting main loop");
	while (true) {
		WAIT(0);
	}
}

static HRESULT Hook_CoCreateInstance(
	REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv
	) {
	HRESULT result = oCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

	char buffer[64];
	GUIDToString(rclsid, buffer, 64);
	LOG("CoCreateInstance: ", buffer);


	if (IsEqualCLSID(rclsid, CLSID_CMSH264EncoderMFT)) {
		IUnknown* unk = (IUnknown*)(*ppv);
		IMFTransform* h264encoder;

		if (SUCCEEDED(unk->QueryInterface<IMFTransform>(&h264encoder))) {
			session = new Encoder::Session();
			if (session != NULL) {
				LOG("Session created.")
			}
		}

		LOG_IF_FAILED(hookVirtualFunction((*ppv), 23, &Hook_IMFTransform_ProcessMessage, &oIMFTransform_ProcessMessage, hkIMFTransform_ProcessMessage), "Failed to hook IMFTransform::ProcessMessage");
		LOG_IF_FAILED(hookVirtualFunction((*ppv), 24, &Hook_IMFTransform_ProcessInput, &oIMFTransform_ProcessInput, hkIMFTransform_ProcessInput), "Failed to hook IMFTransform::ProcessInput");
	}

	return result;
}

static HRESULT Hook_MFCreateSinkWriterFromURL(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	) {
	HRESULT result = oMFCreateSinkWriterFromURL(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	if (SUCCEEDED(result)) {
		LOG_IF_FAILED(hookVirtualFunction(*ppSinkWriter, 3, &Hook_IMFSinkWriter_AddStream, &oIMFSinkWriter_AddStream, hkIMFSinkWriter_AddStream), "Failed to hook IMFSinkWriter::AddStream");
		LOG_IF_FAILED(hookVirtualFunction(*ppSinkWriter, 4, &IMFSinkWriter_SetInputMediaType, &oIMFSinkWriter_SetInputMediaType, hkIMFSinkWriter_SetInputMediaType), "Failed to hook IMFSinkWriter::SetInputMediaType");
		LOG_IF_FAILED(hookVirtualFunction(*ppSinkWriter, 6, &Hook_IMFSinkWriter_WriteSample, &oIMFSinkWriter_WriteSample, hkIMFSinkWriter_WriteSample), "Failed to hook IMFSinkWriter::WriteSample");
		LOG_IF_FAILED(hookVirtualFunction(*ppSinkWriter, 11, &Hook_IMFSinkWriter_Finalize, &oIMFSinkWriter_Finalize, hkIMFSinkWriter_Finalize), "Failed to hook IMFSinkWriter::Finalize");
	}

	return result;
}

static HRESULT Hook_IMFTransform_ProcessMessage(
	IMFTransform     *pThis,
	MFT_MESSAGE_TYPE eMessage,
	ULONG_PTR        ulParam
	) {
	if (eMessage == MFT_MESSAGE_NOTIFY_START_OF_STREAM) {
		LOG("MFT_MESSAGE_NOTIFY_START_OF_STREAM");
		IMFMediaType* pType;
		pThis->GetInputCurrentType(0, &pType);

		if ((pType != NULL) && (session != NULL)) {
			LOG("Input media type: ",GetMediaTypeDescription(pType).c_str());
			if (session->videoCodecContext == NULL) {
				char buffer[MAX_PATH];
				std::stringstream stream = Config::instance().outputDir();

				stream << "\\";
				stream << "XVX-";
				time_t rawtime;
				struct tm timeinfo;
				time(&rawtime);
				localtime_s(&timeinfo, &rawtime);
				strftime(buffer, 2048, "%Y%m%d%H%M%S", &timeinfo);
				stream << buffer;
				stream << ".avi";

				std::string filename = std::regex_replace(stream.str(), std::regex("\\\\+"), "\\");

				LOG("Output file: ", filename);

				UINT width, height, fps_num, fps_den;
				MFGetAttribute2UINT32asUINT64(pType, MF_MT_FRAME_SIZE, &width, &height);
				MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &fps_num, &fps_den);

				GUID pixelFormat;
				pType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

				if (Config::instance().isUseD3DCaptureEnabled()) {
					//latestImage.Initialize2D(DXGI_FORMAT_B8G8R8A8_TYPELESS, width, height, )
					LOG_CALL(session->createVideoContext(width, height, AV_PIX_FMT_ARGB, fps_num, fps_den, AV_PIX_FMT_BGRA));
					LOG_CALL(session->createFormatContext(filename.c_str()));
				} else if (IsEqualGUID(pixelFormat, MFVideoFormat_IYUV)) {
					LOG_CALL(session->createVideoContext(width, height, AV_PIX_FMT_YUV420P, fps_num, fps_den, AV_PIX_FMT_YUV420P));
					LOG_CALL(session->createFormatContext(filename.c_str()));
				} else if (IsEqualGUID(pixelFormat, MFVideoFormat_NV12)) {
					LOG_CALL(session->createVideoContext(width, height, AV_PIX_FMT_NV12, fps_num, fps_den, AV_PIX_FMT_YUV420P));
					LOG_CALL(session->createFormatContext(filename.c_str()));
				}
				else {
					char buffer[64];
					GUIDToString(pixelFormat, buffer, 64);
					LOG("Unknown pixel format specified: ", buffer);
				}
			}
		}

		pType->Release();

	} else if ((eMessage == MFT_MESSAGE_NOTIFY_END_OF_STREAM) && (session != NULL)) {
		LOG("MFT_MESSAGE_NOTIFY_END_OF_STREAM");
		LOG_CALL(session->finishVideo());
		if (session->isSessionFinished) {
			delete session;
			session = NULL;
		}
	}

	return oIMFTransform_ProcessMessage(pThis, eMessage, ulParam);
}

static HRESULT Hook_IMFTransform_ProcessInput(
	IMFTransform	*pThis,
	DWORD			dwInputStreamID,
	IMFSample		*pSample,
	DWORD			dwFlags
	) {

	IMFMediaType* mediaType;
	pThis->GetInputCurrentType(dwInputStreamID, &mediaType);
	LOG("IMFTransform::ProcessInput: ", GetMediaTypeDescription(mediaType).c_str());
	mediaType->Release();

	if (session != NULL) {
		DWORD length;

		BYTE *pData = NULL;

		LONGLONG sampleTime;
		pSample->GetSampleTime(&sampleTime);
		
		if (Config::instance().isUseD3DCaptureEnabled()) {
			std::lock_guard<std::mutex> lock_guard(mxLatestImage);
			if (latestImage.GetImageCount() == 0) {
				LOG("There is no image to capture.");
			}
			const DirectX::Image* image = latestImage.GetImage(0, 0, 0);
			RET_IF_NULL(image, "Could not get current frame.", E_FAIL);
			RET_IF_NULL(image->pixels, "Could not get current frame.", E_FAIL);


			LOG_CALL(session->writeVideoFrame(image->pixels, image->width*image->height*4, sampleTime));
		} else {
			IMFMediaBuffer *mediaBuffer = NULL;
			RET_IF_FAILED(pSample->ConvertToContiguousBuffer(&mediaBuffer), "Could not convert IMFSample to contagiuous buffer", E_FAIL);
			RET_IF_FAILED(mediaBuffer->GetCurrentLength(&length), "Could not get buffer length", E_FAIL);
			RET_IF_FAILED(mediaBuffer->Lock(&pData, NULL, NULL), "Could not lock the buffer", E_FAIL);

			LOG_CALL(session->writeVideoFrame(pData, length, sampleTime));
			LOG_IF_FAILED(mediaBuffer->Unlock(), "Could not unlock the video buffer");
			mediaBuffer->Release();
		}
	}

	return oIMFTransform_ProcessInput(pThis, dwInputStreamID, pSample, dwFlags);
}

static HRESULT Hook_IMFSinkWriter_AddStream(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	) {
	LOG("IMFSinkWriter::AddStream: ", GetMediaTypeDescription(pTargetMediaType).c_str());
	return oIMFSinkWriter_AddStream(pThis, pTargetMediaType, pdwStreamIndex);
}

static HRESULT IMFSinkWriter_SetInputMediaType(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters
	) {
	LOG("IMFSinkWriter::SetInputMediaType: ", GetMediaTypeDescription(pInputMediaType).c_str());

	GUID majorType;
	if (SUCCEEDED(pInputMediaType->GetMajorType(&majorType))) {
		if (IsEqualGUID(majorType, MFMediaType_Audio)) {
			UINT32 blockAlignment, numChannels, sampleRate, bitsPerSample;
			GUID subType;

			pInputMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
			pInputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
			pInputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
			pInputMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
			pInputMediaType->GetGUID(MF_MT_SUBTYPE, &subType);

			if (IsEqualGUID(subType, MFAudioFormat_PCM)) {
				LOG_CALL(session->createAudioContext(numChannels, sampleRate, bitsPerSample, AV_SAMPLE_FMT_S16, blockAlignment));
			}
			else {
				char buffer[64];
				GUIDToString(subType, buffer, 64);
				LOG("Unsupported input audio format: ", buffer);
			}
		}
	}

	return oIMFSinkWriter_SetInputMediaType(pThis, dwStreamIndex, pInputMediaType, pEncodingParameters);
}

static HRESULT Hook_IMFSinkWriter_WriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample
	) {

	if (dwStreamIndex == 1) {
		IMFMediaBuffer *pBuffer;
		LONGLONG sampleTime;
		pSample->GetSampleTime(&sampleTime);
		pSample->ConvertToContiguousBuffer(&pBuffer);

		DWORD length;
		pBuffer->GetCurrentLength(&length);
		BYTE *buffer;
		if (SUCCEEDED(pBuffer->Lock(&buffer, NULL, NULL))) {
			LOG_CALL(session->writeAudioFrame(buffer, length, sampleTime));
		}
		pBuffer->Unlock();
		pBuffer->Release();
	}

	HRESULT result = oIMFSinkWriter_WriteSample(pThis, dwStreamIndex, pSample);
	return result;
}

static HRESULT Hook_IMFSinkWriter_Finalize(
	IMFSinkWriter *pThis
	) {
	LOG_CALL(session->finishAudio());
	if (session->isSessionFinished) {
		delete session;
		session = NULL;
	}
	return oIMFSinkWriter_Finalize(pThis);
}


void unhookAll() {
	hkCoCreateInstance->UnHook();
	hkMFCreateSinkWriterFromURL->UnHook();
	hkIMFTransform_ProcessInput->UnHook();
	hkIMFTransform_ProcessMessage->UnHook();
	hkIMFSinkWriter_AddStream->UnHook();
	hkIMFSinkWriter_SetInputMediaType->UnHook();
	hkIMFSinkWriter_WriteSample->UnHook();
	hkIMFSinkWriter_Finalize->UnHook();
}