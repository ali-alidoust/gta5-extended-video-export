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
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_SetOutputType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_SetInputType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessInput(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessMessage(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_AddStream(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFMediaType_SetUINT32(new PLH::VFuncDetour);

	std::shared_ptr<PLH::IATHook> hkCoCreateInstance(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateSinkWriterFromURL(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> khMFTEnum(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateMediaType(new PLH::IATHook);
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

static HRESULT WINAPI Hook_CoCreateInstance(
	_In_  REFCLSID  rclsid,
	_In_  LPUNKNOWN pUnkOuter,
	_In_  DWORD     dwClsContext,
	_In_  REFIID    riid,
	_Out_ LPVOID    *ppv
	);

static HRESULT WINAPI Hook_MFTEnum(
	GUID                   guidCategory,
	UINT32                 Flags,
	MFT_REGISTER_TYPE_INFO *pInputType,
	MFT_REGISTER_TYPE_INFO *pOutputType,
	IMFAttributes          *pAttributes,
	CLSID                  **ppclsidMFT,
	UINT32                 *pcMFTs
	);

static HRESULT WINAPI Hook_MFCreateMediaType(
	IMFMediaType **ppMFType
	);

static HRESULT __fastcall Hook_IMFMediaType_SetUINT32(
	IMFMediaType* pThis,
	REFGUID guidKey,
	UINT32  unValue
	);

static HRESULT __fastcall Hook_IMFTransform_SetOutputType(
	IMFTransform* pThis,
	DWORD         dwOutputStreamID,
	IMFMediaType  *pType,
	WORD          dwFlags
	);

static HRESULT __fastcall Hook_IMFTransform_ProcessMessage(
	_In_ IMFTransform     *pThis,
	_In_ MFT_MESSAGE_TYPE eMessage,
	_In_ ULONG_PTR        ulParam
	);

static HRESULT __fastcall Hook_IMFTransform_SetInputType(
	_In_ IMFTransform* pThis,
	_In_ DWORD         dwOutputStreamID,
	_In_ IMFMediaType  *pType,
	_In_ WORD          dwFlags
	);

static HRESULT __fastcall Hook_IMFSinkWriter_AddStream(
	IMFSinkWriter* pThis,
	IMFMediaType *pTargetMediaType,
	DWORD        *pdwStreamIndex
	);

static HRESULT __fastcall Hook_IMFTransform_ProcessInput(
	_In_ IMFTransform	*pThis,
	_In_ DWORD			dwInputStreamID,
	_In_ IMFSample		*pSample,
	_In_ DWORD			dwFlags
	);


typedef HRESULT(WINAPI *tMFCreateSinkWriterFromURL)(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	);

typedef HRESULT(WINAPI *tMFTEnum)(
	GUID                   guidCategory,
	UINT32                 Flags,
	MFT_REGISTER_TYPE_INFO *pInputType,
	MFT_REGISTER_TYPE_INFO *pOutputType,
	IMFAttributes          *pAttributes,
	CLSID                  **ppclsidMFT,
	UINT32                 *pcMFTs
	);

typedef HRESULT(__fastcall *tIMFTransform_ProcessInput)(
	_In_ IMFTransform	*pThis,
	_In_ DWORD     dwInputStreamID,
	_In_ IMFSample *pSample,
	_In_ DWORD     dwFlags
	);

typedef HRESULT(__fastcall *tIMFTransform_ProcessMessage)(
	_In_ IMFTransform     *pThis,
	_In_ MFT_MESSAGE_TYPE eMessage,
	_In_ ULONG_PTR        ulParam
	);

typedef HRESULT(WINAPI *tCoCreateInstance)(
	_In_  REFCLSID  rclsid,
	_In_  LPUNKNOWN pUnkOuter,
	_In_  DWORD     dwClsContext,
	_In_  REFIID    riid,
	_Out_ LPVOID    *ppv
	);

typedef HRESULT(WINAPI *tMFCreateMediaType)(
	IMFMediaType **ppMFType
	);

typedef HRESULT(__thiscall *tIMFSinkWriter_AddStream)(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	);

typedef HRESULT(__thiscall *tIMFTransform_SetOutputType)(
	IMFTransform  *pThis,
	DWORD         dwOutputStreamID,
	IMFMediaType  *pType,
	WORD          dwFlags
	);

typedef HRESULT(__thiscall *tIMFTransform_SetInputType)(
	_In_ IMFTransform  *pThis,
	_In_ DWORD         dwOutputStreamID,
	_In_ IMFMediaType  *pType,
	_In_ WORD          dwFlags
	);

typedef HRESULT(__thiscall *tIMFMediaType_SetUINT32)(
	IMFMediaType *pThis,
	REFGUID		 guidKey,
	UINT32		 unValue
	);

tIMFTransform_SetOutputType oIMFTransform_SetOutputType;
tIMFTransform_SetInputType oIMFTransform_SetInputType;
tIMFTransform_ProcessInput oIMFTransform_ProcessInput;
tIMFTransform_ProcessMessage oIMFTransform_ProcessMessage;
tIMFSinkWriter_AddStream oIMFSinkWriter_AddStream;
tIMFMediaType_SetUINT32 oIMFMediaType_SetUINT32;
tCoCreateInstance oCoCreateInstance;
tMFCreateSinkWriterFromURL oMFCreateSinkWriterFromURL;
tMFTEnum oMFTEnum;
tMFCreateMediaType oMFCreateMediaType;

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
	//hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL, &oMFCreateSinkWriterFromURL, IATHook_MFCreateSinkWriterFromURL);
	//hookNamedFunction("mfplat.dll", "MFTEnum", &Hook_MFTEnum, &oMFTEnum, IATHook_MFTEnum);
	//hookNamedFunction("mfplat.dll", "MFCreateMediaType", &Hook_MFCreateMediaType, &oMFCreateMediaType, IATHook_MFCreateMediaType);
	hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance, hkCoCreateInstance);
	av_register_all();
	avcodec_register_all();
	av_log_set_level(AV_LOG_TRACE);
	av_log_set_callback(&avlog_callback);
	while (true) {
		WAIT(0);
	}
}

static HRESULT WINAPI Hook_CoCreateInstance(
	_In_  REFCLSID  rclsid,
	_In_  LPUNKNOWN pUnkOuter,
	_In_  DWORD     dwClsContext,
	_In_  REFIID    riid,
	_Out_ LPVOID    *ppv
	) {
	HRESULT result = oCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

	if (IsEqualCLSID(rclsid, CLSID_CMSH264EncoderMFT)) {
		IUnknown* unk = (IUnknown*)(*ppv);
		IMFTransform* h264encoder;

		if (SUCCEEDED(unk->QueryInterface<IMFTransform>(&h264encoder))) {
			Encoder::createSession(h264encoder);
		}

		hookVirtualFunction((*ppv), 15, &Hook_IMFTransform_SetInputType, &oIMFTransform_SetInputType, hkIMFTransform_SetInputType);
		hookVirtualFunction((*ppv), 23, &Hook_IMFTransform_ProcessMessage, &oIMFTransform_ProcessMessage, hkIMFTransform_ProcessMessage);
		hookVirtualFunction((*ppv), 24, &Hook_IMFTransform_ProcessInput, &oIMFTransform_ProcessInput, hkIMFTransform_ProcessInput);

		
	}


	return result;
}

static HRESULT WINAPI Hook_MFCreateMediaType(
	IMFMediaType **ppMFType
	) {
	HRESULT result = oMFCreateMediaType(ppMFType);

	hookVirtualFunction(*ppMFType, 21, &Hook_IMFMediaType_SetUINT32, &oIMFMediaType_SetUINT32, hkIMFMediaType_SetUINT32);
	return result;
}

static HRESULT WINAPI Hook_MFTEnum(
	GUID                   guidCategory,
	UINT32                 Flags,
	MFT_REGISTER_TYPE_INFO *pInputType,
	MFT_REGISTER_TYPE_INFO *pOutputType,
	IMFAttributes          *pAttributes,
	CLSID                  **ppclsidMFT,
	UINT32                 *pcMFTs
	) {

	HRESULT result = oMFTEnum(guidCategory,
		Flags,
		pInputType,
		pOutputType,
		pAttributes,
		ppclsidMFT,
		pcMFTs);

	return result;
}

static HRESULT WINAPI Hook_MFCreateSinkWriterFromURL(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	) {
	HRESULT result = oMFCreateSinkWriterFromURL(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	if (SUCCEEDED(result)) {
		hookVirtualFunction(*ppSinkWriter, 3, &Hook_IMFSinkWriter_AddStream, &oIMFSinkWriter_AddStream, hkIMFSinkWriter_AddStream);
	}

	return result;
}

static HRESULT __fastcall Hook_IMFTransform_ProcessMessage(
	_In_ IMFTransform     *pThis,
	_In_ MFT_MESSAGE_TYPE eMessage,
	_In_ ULONG_PTR        ulParam
	) {
	if (eMessage == MFT_MESSAGE_NOTIFY_START_OF_STREAM) {
		IMFMediaType* pType;
		pThis->GetInputCurrentType(0, &pType);

		

		if ((pType != NULL) && Encoder::hasSession(pThis)) {
			LOG("Input media type: ",GetMediaTypeDescription(pType).c_str());
			//::MessageBoxW(0, L"Hook_IMFTransform_SetInputType", L"HookTest", MB_OK);
			Encoder::Session *session;
			Encoder::getSession(pThis, &session);
			if (session->codecContext == NULL) {
				CHAR buffer[2048];
				LOG_IF_FAILED(GetVideosDirectory(buffer), "Failed to get Videos directory for the current user.", E_FAIL);
				std::stringstream stream;
				stream << buffer;
				//stream.write(buffer, strlen(pStr));
				//::MessageBoxA(0, stream.str().c_str(), "Hello", 0);
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
					session->createContext(stream.str().c_str(), width, height, AV_PIX_FMT_YUV420P, fps_num, fps_den);
				} else if (IsEqualGUID(pixelFormat, MFVideoFormat_NV12)) {
					session->createContext(stream.str().c_str(), width, height, AV_PIX_FMT_NV12, fps_num, fps_den);
				}
				else {
					LPOLESTR guidStr = NULL;
					StringFromCLSID(pixelFormat, &guidStr);
					LOG("Unknown pixel format specified: ", guidStr);
					CoTaskMemFree(guidStr);
				}
			}
		}

	} else if ((eMessage == MFT_MESSAGE_NOTIFY_END_OF_STREAM) && Encoder::hasSession(pThis)) {
		Encoder::Session *session;
		Encoder::getSession(pThis, &session);
		session->endSession();
		Encoder::deleteSession(pThis);
	}

	return oIMFTransform_ProcessMessage(pThis, eMessage, ulParam);
}

static HRESULT __fastcall Hook_IMFTransform_ProcessInput(
	_In_ IMFTransform	*pThis,
	_In_ DWORD			dwInputStreamID,
	_In_ IMFSample		*pSample,
	_In_ DWORD			dwFlags
	) {

	if (Encoder::hasSession(pThis)) {
		Encoder::Session *session;
		Encoder::getSession(pThis, &session);

		IMFMediaBuffer *mediaBuffer = NULL;
		DWORD length;

		BYTE *pData = NULL;

		RET_IF_FAILED(pSample->ConvertToContiguousBuffer(&mediaBuffer), "Could not convert IMFSample to contagiuous buffer", E_FAIL);
		RET_IF_FAILED(mediaBuffer->GetCurrentLength(&length), "Could not get buffer length", E_FAIL);
		RET_IF_FAILED(mediaBuffer->Lock(&pData, NULL, NULL), "Could not lock the buffer", E_FAIL);
		
		session->writeFrame(pData, length);

		LOG_IF_FAILED(mediaBuffer->Unlock(), "Could not unlock the buffer");
	}

	return oIMFTransform_ProcessInput(pThis, dwInputStreamID, pSample, dwFlags);
}

static HRESULT __fastcall Hook_IMFMediaType_SetUINT32(
	IMFMediaType* pThis,
	REFGUID guidKey,
	UINT32  unValue
	) {

	if (IsEqualGUID(guidKey, MF_MT_AVG_BITRATE)) {
		// ::MessageBoxW(0, L"Hook_IMFMediaType_SetUINT32(MF_MT_AVG_BITRATE)", L"HookTest", MB_OK);
		// unValue = 1000000;
	}

	HRESULT result = oIMFMediaType_SetUINT32(pThis, guidKey, unValue);

	return result;
}

static HRESULT __fastcall Hook_IMFTransform_SetInputType(
	_In_ IMFTransform* pThis,
	_In_ DWORD         dwOutputStreamID,
	_In_ IMFMediaType  *pType,
	_In_ WORD          dwFlags
	) {

	

	

	return oIMFTransform_SetInputType(pThis, dwOutputStreamID, pType, dwFlags);
}

static HRESULT __fastcall Hook_IMFTransform_SetOutputType(
	IMFTransform  *pThis,
	DWORD         dwOutputStreamID,
	IMFMediaType  *pType,
	WORD          dwFlags
	) {
	/*wchar_t buffer[1024];
	wsprintfW(buffer, L"%d \r\n %d \r\n %d \r\n %d", (QWORD)pThis, (QWORD)dwOutputStreamID, (QWORD)pType, (QWORD)dwFlags);
	::MessageBoxW(0, buffer, L"HookTest", MB_OK);*/

	//::MessageBoxW(0, L"Hello!", L"HookTest", MB_OK);

	if (pType != NULL) {
		/*MFCreateMediaType(&pType);
		MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, 1600, 900);
		MFSetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, 30000, 1001);
		pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		pType->SetUINT32(MF_MT_AVG_BITRATE, 10000);
		pType->SetUINT32(MF_MT_INTERLACE_MODE, 2);*/

		//pType->SetUINT32(MF_MT_AVG_BITRATE, 10000);

		//::MessageBoxA(0, GetMediaTypeDescription(pType).c_str(), "Test", MB_OK);
		//pType->SetUINT32(CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_Quality);
		//pType->SetUINT32(CODECAPI_AVEncVideoEncodeQP, 0);
		//pType->SetUINT32(MF_MT_AVG_BITRATE, 16000);
		//pType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_ConstrainedBase);
		//MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, 200, 1);
	}
	return oIMFTransform_SetOutputType(pThis, dwOutputStreamID, pType, dwFlags);
}

static HRESULT __fastcall Hook_IMFSinkWriter_AddStream(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	) {
	//MFSetAttributeRatio(pTargetMediaType, MF_MT_FRAME_RATE, 10, 1);
	//pTargetMediaType->SetUINT32(CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_UnconstrainedVBR);
	//pTargetMediaType->SetUINT32(CODECAPI_AVEncCommonMeanBitRate, 1000);
	//pTargetMediaType->SetUINT32(MF_MT_AVG_BITRATE, 10000);
	return oIMFSinkWriter_AddStream(pThis, pTargetMediaType, pdwStreamIndex);
}



void unhookAllVirtual() {
	hkIMFTransform_ProcessInput->UnHook();
	hkIMFTransform_ProcessMessage->UnHook();
	hkIMFTransform_SetOutputType->UnHook();
	hkIMFTransform_SetInputType->UnHook();
	hkIMFSinkWriter_AddStream->UnHook();
	hkIMFMediaType_SetUINT32->UnHook();
	hkMFCreateSinkWriterFromURL->UnHook();
	khMFTEnum->UnHook();
	hkMFCreateMediaType->UnHook();
	hkCoCreateInstance->UnHook();
}