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

#include <ShlObj.h>

#include "script.h"
#include <string>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include "MFUtility.h"
#include "encoder.h"
#include "logger.h"


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
}

HRESULT GetVideosDirectory(LPSTR output)
{
	PWSTR vidPath = NULL;

	RET_IF_FAILED((SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &vidPath) != S_OK), "Failed to get Videos directory for the current user.", E_FAIL);

	int pathlen = lstrlenW(vidPath);

	int buflen = WideCharToMultiByte(CP_UTF8, 0, vidPath, pathlen, NULL, 0, NULL, NULL);
	if (buflen <= 0)
	{
		return E_FAIL;
	}

	buflen = WideCharToMultiByte(CP_UTF8, 0, vidPath, pathlen, output, buflen, NULL, NULL);

	output[buflen] = 0;

	CoTaskMemFree(vidPath);

	return S_OK;
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
	Logger::instance().write("AVCODEC: ");
	if (ptr)
	{
		AVClass *avc = *(AVClass**)ptr;
		Logger::instance().write("(");
		Logger::instance().write(avc->item_name(ptr));
		Logger::instance().write("): ");
	}
	Logger::instance().write(msg);
}


void ScriptMain() {
	hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL, &oMFCreateSinkWriterFromURL, hkMFCreateSinkWriterFromURL);
	hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance, hkCoCreateInstance);

	av_register_all();
	avcodec_register_all();
	av_log_set_level(AV_LOG_TRACE);
	av_log_set_callback(&avlog_callback);

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
		}

		hookVirtualFunction((*ppv), 23, &Hook_IMFTransform_ProcessMessage, &oIMFTransform_ProcessMessage, hkIMFTransform_ProcessMessage);
		hookVirtualFunction((*ppv), 24, &Hook_IMFTransform_ProcessInput, &oIMFTransform_ProcessInput, hkIMFTransform_ProcessInput);
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
		hookVirtualFunction(*ppSinkWriter, 3, &Hook_IMFSinkWriter_AddStream, &oIMFSinkWriter_AddStream, hkIMFSinkWriter_AddStream);
		hookVirtualFunction(*ppSinkWriter, 4, &IMFSinkWriter_SetInputMediaType, &oIMFSinkWriter_SetInputMediaType, hkIMFSinkWriter_SetInputMediaType);
		hookVirtualFunction(*ppSinkWriter, 6, &Hook_IMFSinkWriter_WriteSample, &oIMFSinkWriter_WriteSample, hkIMFSinkWriter_WriteSample);
		hookVirtualFunction(*ppSinkWriter, 11, &Hook_IMFSinkWriter_Finalize, &oIMFSinkWriter_Finalize, hkIMFSinkWriter_Finalize);
	}

	return result;
}

static HRESULT Hook_IMFTransform_ProcessMessage(
	IMFTransform     *pThis,
	MFT_MESSAGE_TYPE eMessage,
	ULONG_PTR        ulParam
	) {
	if (eMessage == MFT_MESSAGE_NOTIFY_START_OF_STREAM) {
		IMFMediaType* pType;
		pThis->GetInputCurrentType(0, &pType);

		if ((pType != NULL) && (session != NULL)) {
			LOG("Input media type: ",GetMediaTypeDescription(pType).c_str());
			if (session->videoCodecContext == NULL) {
				char buffer[MAX_PATH];
				LOG_IF_FAILED(GetVideosDirectory(buffer), "Failed to get Videos directory for the current user.", E_FAIL);
				std::stringstream stream;
				stream << buffer;
				stream << "\\";
				stream << "XVX-";
				time_t rawtime;
				struct tm timeinfo;
				time(&rawtime);
				localtime_s(&timeinfo, &rawtime);
				strftime(buffer, 2048, "%Y%m%d%H%M%S", &timeinfo);
				stream << buffer;
				stream << ".avi";



				UINT width, height, fps_num, fps_den;
				MFGetAttribute2UINT32asUINT64(pType, MF_MT_FRAME_SIZE, &width, &height);
				MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &fps_num, &fps_den);

				GUID pixelFormat;
				pType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

				if (IsEqualGUID(pixelFormat, MFVideoFormat_IYUV)) {
					session->createVideoContext(width, height, AV_PIX_FMT_YUV420P, fps_num, fps_den);
					session->createFormatContext(stream.str().c_str());
				} else if (IsEqualGUID(pixelFormat, MFVideoFormat_NV12)) {
					session->createVideoContext(width, height, AV_PIX_FMT_NV12, fps_num, fps_den);
					session->createFormatContext(stream.str().c_str());
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
		session->finishVideo();
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
		IMFMediaBuffer *mediaBuffer = NULL;
		DWORD length;

		BYTE *pData = NULL;

		LONGLONG sampleTime;
		pSample->GetSampleTime(&sampleTime);
		RET_IF_FAILED(pSample->ConvertToContiguousBuffer(&mediaBuffer), "Could not convert IMFSample to contagiuous buffer", E_FAIL);
		RET_IF_FAILED(mediaBuffer->GetCurrentLength(&length), "Could not get buffer length", E_FAIL);
		RET_IF_FAILED(mediaBuffer->Lock(&pData, NULL, NULL), "Could not lock the buffer", E_FAIL);
		
		session->writeVideoFrame(pData, length, sampleTime);

		LOG_IF_FAILED(mediaBuffer->Unlock(), "Could not unlock the video buffer");
		mediaBuffer->Release();
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
				session->createAudioContext(numChannels, sampleRate, bitsPerSample, AV_SAMPLE_FMT_S16, blockAlignment);
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
			session->writeAudioFrame(buffer, length, sampleTime);
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
	session->finishAudio();
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