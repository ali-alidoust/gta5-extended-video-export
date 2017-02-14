// script.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "custom-hooks.h"
#include "script.h"
#include "MFUtility.h"
#include "encoder.h"
#include "logger.h"
#include "util.h"
#include "yara-helper.h"
#include "game-detour-def.h"
#include <DirectXMath.h>
#include "VSFullScreen.h"
#include "PSAccumulate.h"
#include "PSDivide.h"
//#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\comdecl.h>
//#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2.h>
//#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\XAudio2fx.h>
//#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\XAPOFX.h>
#pragma warning(push)
#pragma warning( disable : 4005 )
//#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\x3daudio.h>
#pragma warning(pop)
//#pragma comment(lib,"x3daudio.lib")
//#pragma comment(lib,"xapofx.lib")


#include "..\DirectXTex\DirectXTex\DirectXTex.h"
#include "hook-def.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace {
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_AddStream(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_SetInputMediaType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_WriteSample(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_Finalize(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkOMSetRenderTargets(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkDraw(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateSourceVoice(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkSubmitSourceBuffer(new PLH::VFuncDetour);
	
	std::shared_ptr<PLH::IATHook> hkCoCreateInstance(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateSinkWriterFromURL(new PLH::IATHook);

	std::shared_ptr<PLH::X64Detour> hkGetFrameRateFraction(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetRenderTimeBase(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkStepAudio(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetGameSpeedMultiplier(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateThread(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetFrameRate(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetAudioSamples(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkUnk01(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateTexture(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateExportTexture(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkLinearizeTexture(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkAudioUnk01(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkWaitForSingleObject(new PLH::X64Detour);

	/*std::shared_ptr<PLH::X64Detour> hkGetGlobalVariableIndex(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVariable(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetMatrices(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVar(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVarPtrByHash(new PLH::X64Detour);*/
	ComPtr<ID3D11Texture2D> pGameBackBuffer;
	ComPtr<ID3D11Texture2D> pGameBackBufferResolved;
	ComPtr<ID3D11Texture2D> pGameDepthBufferQuarterLinear;
	ComPtr<ID3D11Texture2D> pGameDepthBufferQuarter;
	ComPtr<ID3D11Texture2D> pGameDepthBufferResolved;
	ComPtr<ID3D11Texture2D> pGameDepthBuffer;
	ComPtr<ID3D11Texture2D> pGameGBuffer0;
	ComPtr<ID3D11Texture2D> pGameEdgeCopy;
	ComPtr<ID3D11Texture2D> pLinearDepthTexture;
	ComPtr<ID3D11Texture2D> pStencilTexture;

	ComPtr<ID3D11DeviceContext> pDContext;
	
	
	DWORD threadIdRageAudioMixThread = 0;
	ComPtr<ID3D11Texture2D> pMotionBlurAccBuffer;
	ComPtr<ID3D11Texture2D> pMotionBlurFinalBuffer;
	ComPtr<ID3D11VertexShader> pVSFullScreen;
	ComPtr<ID3D11PixelShader> pPSAccumulate;
	ComPtr<ID3D11PixelShader> pPSDivide;
	ComPtr<ID3D11Buffer> pDivideConstantBuffer;
	ComPtr<ID3D11RasterizerState> pRasterState;
	ComPtr<ID3D11SamplerState> pPSSampler;
	ComPtr<ID3D11RenderTargetView> pRTVAccBuffer;
	ComPtr<ID3D11RenderTargetView> pRTVBlurBuffer;
	ComPtr<ID3D11ShaderResourceView> pSRVAccBuffer;
	ComPtr<ID3D11Texture2D> pSourceSRVTexture;
	ComPtr<ID3D11ShaderResourceView> pSourceSRV;
	ComPtr<ID3D11BlendState> pAccBlendState;

	struct PSConstantBuffer {
		XMFLOAT4 floats;
	} cb;

	void* pCtxLinearizeBuffer = NULL;
	//std::shared_ptr<PLH::X64Detour> hkGetTexture(new PLH::X64Detour);
	bool isCustomFrameRateSupported = false;

	std::unique_ptr<Encoder::Session> session;
	std::mutex mxSession;

	void *pGlobalUnk01 = NULL;

	std::thread::id mainThreadId;
	ComPtr<IDXGISwapChain> mainSwapChain;

	ComPtr<IDXGIFactory> pDXGIFactory;

	struct ExportContext {

		ExportContext()
		{
			PRE();			
			POST();
		}

		~ExportContext() {
			PRE();
			POST();
		}

		float audioSkipCounter = 0;
		float audioSkip = 0;

		bool isAudioExportDisabled = false;

		bool captureRenderTargetViewReference = false;
		bool captureDepthStencilViewReference = false;
		
		ComPtr<ID3D11Texture2D> pExportRenderTarget;
		ComPtr<ID3D11DeviceContext> pDeviceContext;
		ComPtr<ID3D11Device> pDevice;
		ComPtr<IDXGIFactory> pDXGIFactory;
		ComPtr<IDXGISwapChain> pSwapChain;

		float far_clip = 0;
		float near_clip = 0;

		std::shared_ptr<DirectX::ScratchImage> capturedImage = std::make_shared<DirectX::ScratchImage>();

		UINT pts = 0;
		uint32_t totalFrameNum = 0;
		uint32_t motionBlurSamples;
		float motion_blur_strength;
		uint32_t accCount = 0;

		ComPtr<IMFMediaType> videoMediaType;

	};

	std::shared_ptr<ExportContext> exportContext;

	std::shared_ptr<YaraHelper> pYaraHelper;

}

tCoCreateInstance oCoCreateInstance;
tMFCreateSinkWriterFromURL oMFCreateSinkWriterFromURL;
tIMFSinkWriter_SetInputMediaType oIMFSinkWriter_SetInputMediaType;
tIMFSinkWriter_AddStream oIMFSinkWriter_AddStream;
tIMFSinkWriter_WriteSample oIMFSinkWriter_WriteSample;
tIMFSinkWriter_Finalize oIMFSinkWriter_Finalize;
tOMSetRenderTargets oOMSetRenderTargets;
tGetRenderTimeBase oGetRenderTimeBase;
//tGetGameSpeedMultiplier oGetGameSpeedMultiplier;
tCreateThread oCreateThread;
//tWaitForSingleObject oWaitForSingleObject;
//tStepAudio oStepAudio;
tCreateTexture oCreateTexture;
//tCreateSourceVoice oCreateSourceVoice;
//tSubmitSourceBuffer oSubmitSourceBuffer;
tDraw oDraw;
tAudioUnk01 oAudioUnk01;

void avlog_callback(void *ptr, int level, const char* fmt, va_list vargs) {
	static char msg[8192];
	vsnprintf_s(msg, sizeof(msg), fmt, vargs);
	Logger::instance().write(
		Logger::instance().getTimestamp(),
		" ",
		Logger::instance().getLogLevelString(LL_NON),
		" ",
		Logger::instance().getThreadId(), " AVCODEC: ");
	if (ptr)
	{
		AVClass *avc = *(AVClass**)ptr;
		Logger::instance().write("(");
		Logger::instance().write(avc->item_name(ptr));
		Logger::instance().write("): ");
	}
	Logger::instance().write(msg);
}

//static HRESULT CreateSourceVoice(
//	IXAudio2                    *pThis,
//	IXAudio2SourceVoice         **ppSourceVoice,
//	const WAVEFORMATEX          *pSourceFormat,
//	UINT32                      Flags,
//	float                       MaxFrequencyRatio,
//	IXAudio2VoiceCallback       *pCallback,
//	const XAUDIO2_VOICE_SENDS   *pSendList,
//	const XAUDIO2_EFFECT_CHAIN  *pEffectChain
//	) {
//	PRE();
//	HRESULT result = oCreateSourceVoice(pThis, ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
//	if (SUCCEEDED(result)) {
//		hookVirtualFunction(*ppSourceVoice, 21, &SubmitSourceBuffer, &oSubmitSourceBuffer, hkSubmitSourceBuffer);
//	}
//	//WAVEFORMATEX *pSourceFormatRW = (WAVEFORMATEX *)pSourceFormat;
//	//pSourceFormatRW->nSamplesPerSec = pSourceFormat->nSamplesPerSec;
//	POST();
//	return result;
//}

//static HRESULT SubmitSourceBuffer(
//	IXAudio2SourceVoice      *pThis,
//	const XAUDIO2_BUFFER     *pBuffer,
//	const XAUDIO2_BUFFER_WMA *pBufferWMA
//	) {
//	//StackDump(128, __func__);
//	return oSubmitSourceBuffer(pThis, pBuffer, pBufferWMA);
//	//return S_OK;
//}

void onPresent(IDXGISwapChain *swapChain) {
	mainSwapChain = swapChain;
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		try {
			ComPtr<ID3D11Device> pDevice;
			ComPtr<ID3D11DeviceContext> pDeviceContext;
			ComPtr<ID3D11Texture2D> texture;
			DXGI_SWAP_CHAIN_DESC desc;


			swapChain->GetDesc(&desc);


			LOG(LL_NFO, "BUFFER COUNT: ", desc.BufferCount);
			REQUIRE(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)texture.GetAddressOf()), "Failed to get the texture buffer");
			REQUIRE(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)pDevice.GetAddressOf()), "Failed to get the D3D11 device");
			pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
			NOT_NULL(pDeviceContext.Get(), "Failed to get D3D11 device context");

			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 13, &Draw, &oDraw, hkDraw), "Failed to hook ID3DDeviceContext::Draw");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 33, &Hook_OMSetRenderTargets, &oOMSetRenderTargets, hkOMSetRenderTargets), "Failed to hook ID3DDeviceContext::OMSetRenderTargets");
			ComPtr<IDXGIDevice> pDXGIDevice;
			REQUIRE(pDevice.As(&pDXGIDevice), "Failed to get IDXGIDevice from ID3D11Device");
			
			ComPtr<IDXGIAdapter> pDXGIAdapter;
			REQUIRE(pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)pDXGIAdapter.GetAddressOf()), "Failed to get IDXGIAdapter");
			REQUIRE(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)pDXGIFactory.GetAddressOf()), "Failed to get IDXGIFactory");

			prepareDeferredContext(pDevice, pDeviceContext);
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}
}

void initialize() {
	PRE();
	try {
		mainThreadId = std::this_thread::get_id();

		/*REQUIRE(CoInitializeEx(NULL, COINIT_MULTITHREADED), "Failed to initialize COM");

		ComPtr<IXAudio2> pXAudio;
		REQUIRE(XAudio2Create(pXAudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR), "Failed to create XAudio2 object");*/
		//REQUIRE(hookVirtualFunction(pXAudio.Get(), 11, &CreateSourceVoice, &oCreateSourceVoice, hkCreateSourceVoice), "Failed to hook IXAudio2::CreateSourceVoice");
		//IXAudio2MasteringVoice *pMasterVoice;
		//WAVEFORMATEX waveFormat;

		//UINT32 deviceCount; // Number of devices
		//pXAudio->GetDeviceCount(&deviceCount);

		//XAUDIO2_DEVICE_DETAILS deviceDetails;	// Structure to hold details about audio device
		//int preferredDevice = 0;				// Device to be used

		//										// Loop through devices 
		//for (unsigned int i = 0; i < deviceCount; i++)
		//{
		//	// Get device details
		//	pXAudio->GetDeviceDetails(i, &deviceDetails);
		//	// Check conditions ( default device )
		//	if (deviceDetails.Role == DefaultCommunicationsDevice)
		//	{
		//		preferredDevice = i;
		//		break;
		//	}

		//}


		REQUIRE(hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL, &oMFCreateSinkWriterFromURL, hkMFCreateSinkWriterFromURL), "Failed to hook MFCreateSinkWriterFromURL in mfreadwrite.dll");
		//REQUIRE(hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance, hkCoCreateInstance), "Failed to hook CoCreateInstance in ole32.dll");
			
		pYaraHelper.reset(new YaraHelper());
		pYaraHelper->initialize();

		MODULEINFO info;
		GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &info, sizeof(info));
		LOG(LL_NFO, "Image base:", ((void*)info.lpBaseOfDll));

		void *pGetRenderTimeBase = nullptr;
		void *pCreateTexture = nullptr;
		void *pLinearizeTexture = nullptr;
		void *pAudioUnk01 = nullptr;
		void *pGetGameSpeedMultiplier = nullptr;
		void *pStepAudio = nullptr;
		void *pCreateThread = nullptr;
		void *pWaitForSingleObject = nullptr;
		pYaraHelper->addEntry("yara_get_render_time_base_function", yara_get_render_time_base_function, &pGetRenderTimeBase);
		//pYaraHelper->addEntry("yara_get_game_speed_multiplier_function", yara_get_game_speed_multiplier_function, &pGetGameSpeedMultiplier);
		//pYaraHelper->addEntry("yara_step_audio_function", yara_step_audio_function, &pStepAudio);
		//pYaraHelper->addEntry("yara_audio_unk01_function", yara_audio_unk01_function, &pAudioUnk01);
		pYaraHelper->addEntry("yara_create_thread_function", yara_create_thread_function, &pCreateThread);
		//pYaraHelper->addEntry("yara_create_export_texture_function", yara_create_export_texture_function, &pCreateExportTexture);
		pYaraHelper->addEntry("yara_create_texture_function", yara_create_texture_function, &pCreateTexture);
		//pYaraHelper->addEntry("yara_wait_for_single_object", yara_wait_for_single_object, &pWaitForSingleObject);
		/*pYaraHelper->addEntry("yara_global_unk01_command", yara_global_unk01_command, &pGlobalUnk01Cmd);
		pYaraHelper->addEntry("yara_get_var_ptr_by_hash_2", yara_get_var_ptr_by_hash_2, &pGetVarPtrByHash);*/
		pYaraHelper->performScan();
		//LOG(LL_NFO, (VOID*)(0x311C84 + (intptr_t)info.lpBaseOfDll));
		
		//REQUIRE(hookX64Function((BYTE*)(0x11441F4 + (intptr_t)info.lpBaseOfDll), &Detour_GetVarHash, &oGetVarHash, hkGetVar), "Failed to hook GetVar function.");
		try {
			if (pGetRenderTimeBase) {
				REQUIRE(hookX64Function(pGetRenderTimeBase, &Detour_GetRenderTimeBase, &oGetRenderTimeBase, hkGetRenderTimeBase), "Failed to hook FPS function.");
				isCustomFrameRateSupported = true;		
			} else {
				LOG(LL_ERR, "Could not find the address for FPS function.");
				LOG(LL_ERR, "Custom FPS support is DISABLED!!!");
			}

			if (pCreateTexture) {
				REQUIRE(hookX64Function(pCreateTexture, &Detour_CreateTexture, &oCreateTexture, hkCreateTexture), "Failed to hook CreateTexture function.");
			} else {
				LOG(LL_ERR, "Could not find the address for CreateTexture function.");
			}

			if (pCreateThread) {
				REQUIRE(hookX64Function(pCreateThread, &Detour_CreateThread, &oCreateThread, hkCreateThread), "Failed to hook CreateThread function.");
			} else {
				LOG(LL_ERR, "Could not find the address for CreateThread function.");
			}

			/*if (pWaitForSingleObject) {
				REQUIRE(hookX64Function(pWaitForSingleObject, &Detour_WaitForSingleObject, &oWaitForSingleObject, hkWaitForSingleObject), "Failed to hook WaitForSingleObject function.");
			} else {
				LOG(LL_ERR, "Could not find the address for WaitForSingleObject function.");
			}*/

			/*if (pStepAudio) {
				REQUIRE(hookX64Function(pStepAudio, &Detour_StepAudio, &oStepAudio, hkStepAudio), "Failed to hook StepAudio function.");
			} else {
				LOG(LL_ERR, "Could not find the address for StepAudio.");
			}
*/
			/*if (pGetGameSpeedMultiplier) {
				REQUIRE(hookX64Function(pGetGameSpeedMultiplier, &Detour_GetGameSpeedMultiplier, &oGetGameSpeedMultiplier, hkGetGameSpeedMultiplier), "Failed to hook GetGameSpeedMultiplier function.");
			} else {
				LOG(LL_ERR, "Could not find the address for GetGameSpeedMultiplier function.");
			}*/

			/*if (pAudioUnk01) {
				REQUIRE(hookX64Function(pAudioUnk01, &Detour_AudioUnk01, &oAudioUnk01, hkAudioUnk01), "Failed to hook AudioUnk01 function.");
			} else {
				LOG(LL_ERR, "Could not find the address for AudioUnk01 function.");
			}*/

			/*if (pCreateExportTexture) {
				REQUIRE(hookX64Function(pCreateExportTexture, &Detour_CreateTexture, &oCreateTexture, hkCreateExportTexture), "Failed to hook CreateExportTexture function.");
			}*/

			/*if (pGlobalUnk01Cmd) {
				uint32_t offset = *(uint32_t*)((uint8_t*)pGlobalUnk01Cmd + 3);
				pGlobalUnk01 = (uint8_t*)pGlobalUnk01Cmd + offset + 7;
				LOG(LL_DBG, "pGlobalUnk01: ", pGlobalUnk01);
			} else {
				LOG(LL_ERR, "Could not find the address for global var: pGlobalUnk0");
			}

			if (pGetVarPtrByHash) {
				REQUIRE(hookX64Function(pGetVarPtrByHash, &Detour_GetVarPtrByHash2, &oGetVarPtrByHash2, hkGetVarPtrByHash), "Failed to hook GetVarPtrByHash function.");
			} else {

			}*/

		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		////0x14CC6AC;
		//REQUIRE(hookX64Function((BYTE*)(0x14CC6AC + (intptr_t)info.lpBaseOfDll), &Detour_GetFrameRate, &oGetFrameRate, hkGetFrameRate), "Failed to hook frame rate function.");
		////0x14CC64C;
		//REQUIRE(hookX64Function((BYTE*)(0x14CC64C + (intptr_t)info.lpBaseOfDll), &Detour_GetAudioSamples, &oGetAudioSamples, hkGetAudioSamples), "Failed to hook audio sample function.");
		//0x14CBED8;
		//REQUIRE(hookX64Function((BYTE*)(0x14CBED8 + (intptr_t)info.lpBaseOfDll), &Detour_getFrameRateFraction, &ogetFrameRateFraction, hkGetFrameRateFraction), "Failed to hook audio sample function.");
		//0x87CC80;
		//REQUIRE(hookX64Function((BYTE*)(0x87CC80 + (intptr_t)info.lpBaseOfDll), &Detour_Unk01, &oUnk01, hkUnk01), "Failed to hook audio sample function.");
		//0x4BB744;
		//0x754884
		//REQUIRE(hookX64Function((BYTE*)(0x754884 + (intptr_t)info.lpBaseOfDll), &Detour_GetTexture, &oGetTexture, hkGetTexture), "Failed to hook GetTexture function.");
		//0x11C4980
		//REQUIRE(hookX64Function((BYTE*)(0x11C4980 + (intptr_t)info.lpBaseOfDll), &Detour_GetGlobalVariableIndex, &oGetGlobalVariableIndex, hkGetGlobalVariableIndex), "Failed to hook GetSomething function.");
		//0x1552198
		//REQUIRE(hookX64Function((BYTE*)(0x1552198 + (intptr_t)info.lpBaseOfDll), &Detour_GetVariable, &oGetVariable, hkGetVariable), "Failed to hook GetVariable function.");
		//REQUIRE(hookX64Function((BYTE*)(0x15546F8 + (intptr_t)info.lpBaseOfDll), &Detour_GetVariable, &oGetVariable, hkGetVariable), "Failed to hook GetVariable function.");
		//11CA828
		//REQUIRE(hookX64Function((BYTE*)(0x11CA828 + (intptr_t)info.lpBaseOfDll), &Detour_GetMatrices, &oGetMatrices, hkGetMatrices), "Failed to hook GetMatrices function.");
		//11441F4
		//0x352A3C
		//REQUIRE(hookX64Function((BYTE*)(0x352A3C + (intptr_t)info.lpBaseOfDll), &Detour_GetVarPtrByHash, &oGetVarPtrByHash, hkGetVarPtrByHash), "Failed to hook GetVarPtrByHash function.");

		LOG_CALL(LL_DBG, av_register_all());
		LOG_CALL(LL_DBG, avcodec_register_all());
		LOG_CALL(LL_DBG, av_log_set_level(AV_LOG_TRACE));
		LOG_CALL(LL_DBG, av_log_set_callback(&avlog_callback));
	} catch (std::exception& ex) {
		// TODO cleanup
		POST();
		throw ex;
	}
	POST();
}

void ScriptMain() {
	PRE();
	LOG(LL_NFO, "Starting main loop");
	while (true) {
		WAIT(0);
	}
	POST();
}

static HRESULT Hook_CoCreateInstance(
	REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv
	) {
	PRE();
	HRESULT result = oCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

	/*if (IsEqualGUID(riid, _uuidof(IXAudio2))) {
		IXAudio2* pXAudio = (IXAudio2*)(*ppv);
		REQUIRE(hookVirtualFunction(pXAudio, 8, &CreateSourceVoice, &oCreateSourceVoice, hkCreateSourceVoice), "Failed to hook IXAudio2::CreateSourceVoice");
	}*/

	char buffer[64];
	GUIDToString(rclsid, buffer, 64);
	LOG(LL_NFO, "CoCreateInstance: ", buffer);
	POST();
	return result;
}

static void Hook_OMSetRenderTargets(
	ID3D11DeviceContext           *pThis,
	UINT                          NumViews,
	ID3D11RenderTargetView *const *ppRenderTargetViews,
	ID3D11DepthStencilView        *pDepthStencilView
	) {

	if (::exportContext) {
		for (uint32_t i = 0; i < NumViews; i++) {
			if (ppRenderTargetViews[i]) {
				ComPtr<ID3D11Resource> pResource;
				ComPtr<ID3D11Texture2D> pTexture2D;
				ppRenderTargetViews[i]->GetResource(pResource.GetAddressOf());
				if (SUCCEEDED(pResource.As(&pTexture2D))) {
					if (pTexture2D.Get() == pGameDepthBufferQuarterLinear.Get()) {
						LOG(LL_DBG, " i:", i, " num:", NumViews, " dsv:", (void*)pDepthStencilView);
						pCtxLinearizeBuffer = pThis;
					}
				}
			}
		}
	}

	ComPtr<ID3D11Resource> pRTVTexture;
	if ((ppRenderTargetViews) && (ppRenderTargetViews[0])) {
		ppRenderTargetViews[0]->GetResource(pRTVTexture.GetAddressOf());
	}

	ComPtr<ID3D11RenderTargetView> pOldRTV;
	ComPtr<ID3D11Resource> pOldRTVTexture;
	pThis->OMGetRenderTargets(1, pOldRTV.GetAddressOf(), NULL);
	if (pOldRTV) {
		pOldRTV->GetResource(pOldRTVTexture.GetAddressOf());
	}

	if ((::exportContext != NULL) && (::exportContext->pExportRenderTarget != NULL) && (::exportContext->pExportRenderTarget == pRTVTexture)) {
		std::lock_guard<std::mutex> sessionLock(mxSession);
		if ((session != NULL) && (session->isCapturing)) {

			// Time to capture rendered frame
			try {
				ComPtr<ID3D11Device> pDevice;
				pThis->GetDevice(pDevice.GetAddressOf());

				ComPtr<ID3D11Texture2D> pDepthBufferCopy = nullptr;
				ComPtr<ID3D11Texture2D> pBackBufferCopy = nullptr;
				ComPtr<ID3D11Texture2D> pStencilBufferCopy = nullptr;
				ComPtr<ID3D11Texture2D> result;
				

				if (config::export_openexr) {
					{
						D3D11_TEXTURE2D_DESC desc;
						pLinearDepthTexture->GetDesc(&desc);

						desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
						desc.BindFlags = 0;
						desc.MiscFlags = 0;
						desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

						REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pDepthBufferCopy.GetAddressOf()), "Failed to create depth buffer copy texture");

						pThis->CopyResource(pDepthBufferCopy.Get(), pLinearDepthTexture.Get());
					}
					{
						D3D11_TEXTURE2D_DESC desc;
						pGameBackBufferResolved->GetDesc(&desc);
						desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
						desc.BindFlags = 0;
						desc.MiscFlags = 0;
						desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

						REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pBackBufferCopy.GetAddressOf()), "Failed to create back buffer copy texture");

						pThis->CopyResource(pBackBufferCopy.Get(), pGameBackBufferResolved.Get());
					}
					// FIXME
					{
						D3D11_TEXTURE2D_DESC desc;
						pStencilTexture->GetDesc(&desc);
						desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
						desc.BindFlags = 0;
						desc.MiscFlags = 0;
						desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

						REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pStencilBufferCopy.GetAddressOf()), "Failed to create stencil buffer copy");

						//pThis->ResolveSubresource(pStencilBufferCopy.Get(), 0, pGameDepthBuffer.Get(), 0, DXGI_FORMAT::DXGI_FORMAT_R32G8X24_TYPELESS);
						pThis->CopyResource(pStencilBufferCopy.Get(), pGameEdgeCopy.Get());
					}
					session->enqueueEXRImage(pThis, pBackBufferCopy, pDepthBufferCopy, pStencilBufferCopy);
				}


				LOG_CALL(LL_DBG, ::exportContext->pSwapChain->Present(0, DXGI_PRESENT_TEST)); // IMPORTANT: This call makes ENB and ReShade effects to be applied to the render target

				ComPtr<ID3D11Texture2D> pSwapChainBuffer;
				REQUIRE(::exportContext->pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pSwapChainBuffer.GetAddressOf()), "Failed to get swap chain's buffer");
				{
					float current_shutter_position = (::exportContext->totalFrameNum % (::exportContext->motionBlurSamples + 1)) / (float)::exportContext->motionBlurSamples;
					if (current_shutter_position >= (1 - ::exportContext->motion_blur_strength)) {
						drawAdditive(pDevice, pThis, pSwapChainBuffer);
					}
					if ((::exportContext->totalFrameNum % (::exportContext->motionBlurSamples + 1)) == ::exportContext->motionBlurSamples) {
						result = divideBuffer(pDevice, pThis, ::exportContext->accCount);
						::exportContext->accCount = 0;
					}
					::exportContext->totalFrameNum++;
				}

				if (result) {
					//auto& image_ref = *(::exportContext->capturedImage);
					//LOG_CALL(LL_DBG, DirectX::CaptureTexture(::exportContext->pDevice.Get(), ::exportContext->pDeviceContext.Get(), result.Get(), image_ref));
					/*if (::exportContext->capturedImage->GetImageCount() == 0) {
						LOG(LL_ERR, "There is no image to capture.");
						throw std::exception();
					}*/
					//const DirectX::Image* image = ::exportContext->capturedImage->GetImage(0, 0, 0);
					//NOT_NULL(image, "Could not get current frame.");
					//NOT_NULL(image->pixels, "Could not get current frame.");
					D3D11_TEXTURE2D_DESC desc;
					result->GetDesc(&desc);

					/*D3D11_MAPPED_SUBRESOURCE mapped;
					REQUIRE(pThis->Map(result.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Failed to map result texture.");*/
					REQUIRE(session->enqueueVideoFrame(pThis, result), "Failed to enqueue frame");
					//::exportContext->capturedImage->Release();
				}
			} catch (std::exception&) {
				LOG(LL_ERR, "Reading video frame from D3D Device failed.");
				::exportContext->capturedImage->Release();
				LOG_CALL(LL_DBG, session.reset());
				LOG_CALL(LL_DBG, ::exportContext.reset());
			}
		}
	}
	oOMSetRenderTargets(pThis, NumViews, ppRenderTargetViews, pDepthStencilView);
}

static HRESULT Hook_MFCreateSinkWriterFromURL(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	) {
	PRE();
	HRESULT result = oMFCreateSinkWriterFromURL(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	if (SUCCEEDED(result)) {
		try {
			REQUIRE(hookVirtualFunction(*ppSinkWriter, 3, &Hook_IMFSinkWriter_AddStream, &oIMFSinkWriter_AddStream, hkIMFSinkWriter_AddStream), "Failed to hook IMFSinkWriter::AddStream");
			REQUIRE(hookVirtualFunction(*ppSinkWriter, 4, &IMFSinkWriter_SetInputMediaType, &oIMFSinkWriter_SetInputMediaType, hkIMFSinkWriter_SetInputMediaType), "Failed to hook IMFSinkWriter::SetInputMediaType");
			REQUIRE(hookVirtualFunction(*ppSinkWriter, 6, &Hook_IMFSinkWriter_WriteSample, &oIMFSinkWriter_WriteSample, hkIMFSinkWriter_WriteSample), "Failed to hook IMFSinkWriter::WriteSample");
			REQUIRE(hookVirtualFunction(*ppSinkWriter, 11, &Hook_IMFSinkWriter_Finalize, &oIMFSinkWriter_Finalize, hkIMFSinkWriter_Finalize), "Failed to hook IMFSinkWriter::Finalize");
		} catch (...) {
			LOG(LL_ERR, "Hooking IMFSinkWriter functions failed");
		}
	}
	POST();
	return result;
}

static HRESULT Hook_IMFSinkWriter_AddStream(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	) {
	PRE();
	LOG(LL_NFO, "IMFSinkWriter::AddStream: ", GetMediaTypeDescription(pTargetMediaType).c_str());
	POST();
	return oIMFSinkWriter_AddStream(pThis, pTargetMediaType, pdwStreamIndex);
}

static HRESULT IMFSinkWriter_SetInputMediaType(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters
	) {
	PRE();
	LOG(LL_NFO, "IMFSinkWriter::SetInputMediaType: ", GetMediaTypeDescription(pInputMediaType).c_str());

	GUID majorType;
	if (SUCCEEDED(pInputMediaType->GetMajorType(&majorType))) {
		if (IsEqualGUID(majorType, MFMediaType_Video)) {
			::exportContext->videoMediaType = pInputMediaType;
		} else if (IsEqualGUID(majorType, MFMediaType_Audio)) {
			try {
				std::lock_guard<std::mutex> sessionLock(mxSession);

				UINT width, height, fps_num, fps_den;
				MFGetAttribute2UINT32asUINT64(::exportContext->videoMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
				MFGetAttributeRatio(::exportContext->videoMediaType.Get(), MF_MT_FRAME_RATE, &fps_num, &fps_den);

				GUID pixelFormat;
				::exportContext->videoMediaType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

				DXGI_SWAP_CHAIN_DESC desc;
				::exportContext->pSwapChain->GetDesc(&desc);


					
				if (isCustomFrameRateSupported) {
					auto fps = config::fps;
					fps_num = fps.first;
					fps_den = fps.second;


					float gameFrameRate = ((float)fps.first * ((float)config::motion_blur_samples + 1) / (float)fps.second);
					if ( gameFrameRate > 60.0f) {
						LOG(LL_NON, "fps * (motion_blur_samples + 1) > 60.0!!!");
						LOG(LL_NON, "Audio export will be disabled!!!");

						//::exportContext->audioSkip = gameFrameRate / 60;
						//::exportContext->audioSkipCounter = ::exportContext->audioSkip;
						::exportContext->isAudioExportDisabled = true;
					}
				}

				//REQUIRE(session->createVideoContext(desc.BufferDesc.Width, desc.BufferDesc.Height, "bgra", fps_num, fps_den, config::motion_blur_samples,  config::video_fmt, config::video_enc, config::video_cfg), "Failed to create video context");

				UINT32 blockAlignment, numChannels, sampleRate, bitsPerSample;
				GUID subType;

				pInputMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
				pInputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
				pInputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
				pInputMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
				pInputMediaType->GetGUID(MF_MT_SUBTYPE, &subType);

				/*if (IsEqualGUID(subType, MFAudioFormat_PCM)) {
					REQUIRE(session->createAudioContext(numChannels, sampleRate, bitsPerSample, "s16", blockAlignment, config::audio_fmt, config::audio_enc, config::audio_cfg), "Failed to create audio context.");
				} else {
					char buffer[64];
					GUIDToString(subType, buffer, 64);
					LOG(LL_ERR, "Unsupported input audio format: ", buffer);
					throw std::runtime_error("Unsupported input audio format");
				}*/

				char buffer[128];
				std::string output_file = config::output_dir;
				std::string exrOutputPath;

				output_file += "\\EVE-";
				time_t rawtime;
				struct tm timeinfo;
				time(&rawtime);
				localtime_s(&timeinfo, &rawtime);
				strftime(buffer, 128, "%Y%m%d%H%M%S", &timeinfo);
				output_file += buffer;
				exrOutputPath = std::regex_replace(output_file, std::regex("\\\\+"), "\\");
				output_file += "." + config::format_ext;

				std::string filename = std::regex_replace(output_file, std::regex("\\\\+"), "\\");

				LOG(LL_NFO, "Output file: ", filename);

				REQUIRE(session->createContext(config::container_format,
					filename.c_str(),
					exrOutputPath,
					config::format_cfg,
					desc.BufferDesc.Width,
					desc.BufferDesc.Height,
					"rgba",
					fps_num,
					fps_den,
					config::motion_blur_samples,
					1 - config::motion_blur_strength,
					config::video_fmt,
					config::video_enc,
					config::video_cfg, 
					numChannels, 
					sampleRate, 
					bitsPerSample,
					"s16", 
					blockAlignment, 
					config::audio_fmt, 
					config::audio_enc, 
					config::audio_cfg), "Failed to create encoding context.");

				::exportContext->motionBlurSamples = config::motion_blur_samples;
				::exportContext->motion_blur_strength = config::motion_blur_strength;
			} catch (std::exception& ex) {
				LOG(LL_ERR, ex.what());
				LOG_CALL(LL_DBG, session.reset());
				LOG_CALL(LL_DBG, ::exportContext.reset());
			}
		}
	}

	POST();
	return oIMFSinkWriter_SetInputMediaType(pThis, dwStreamIndex, pInputMediaType, pEncodingParameters);
}

static HRESULT Hook_IMFSinkWriter_WriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample
	) {
	std::lock_guard<std::mutex> sessionLock(mxSession);

	if ((session != NULL) && (dwStreamIndex == 1) && (!::exportContext->isAudioExportDisabled)) {

		ComPtr<IMFMediaBuffer> pBuffer = NULL;
		try {
			LONGLONG sampleTime;
			pSample->GetSampleTime(&sampleTime);
			pSample->ConvertToContiguousBuffer(pBuffer.GetAddressOf());

			DWORD length;
			pBuffer->GetCurrentLength(&length);
			BYTE *buffer;
			if (SUCCEEDED(pBuffer->Lock(&buffer, NULL, NULL))) {
				LOG_CALL(LL_DBG, session->writeAudioFrame(buffer, length, sampleTime));
			}
			
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
			LOG_CALL(LL_DBG, session.reset());
			LOG_CALL(LL_DBG, ::exportContext.reset());
			if (pBuffer != NULL) {
				pBuffer->Unlock();
			}
		}
	}
	/*if (!session) { 
		return E_FAIL;
	}*/
	return S_OK;
}

static HRESULT Hook_IMFSinkWriter_Finalize(
	IMFSinkWriter *pThis
	) {
	PRE();
	std::lock_guard<std::mutex> sessionLock(mxSession);
	try {
		if (session != NULL) {
			LOG_CALL(LL_DBG, session->finishAudio());
			LOG_CALL(LL_DBG, session->finishVideo());
			LOG_CALL(LL_DBG, session->endSession());
		}
	} catch (std::exception& ex) {
		LOG(LL_ERR, ex.what());
	}

	LOG_CALL(LL_DBG, session.reset());
	LOG_CALL(LL_DBG, ::exportContext.reset());
	POST();
	return S_OK;
}

void finalize() {
	PRE();
	hkCoCreateInstance->UnHook();
	hkMFCreateSinkWriterFromURL->UnHook();
	hkIMFSinkWriter_AddStream->UnHook();
	hkIMFSinkWriter_SetInputMediaType->UnHook();
	hkIMFSinkWriter_WriteSample->UnHook();
	hkIMFSinkWriter_Finalize->UnHook();
	POST();
}

static void Draw(
	ID3D11DeviceContext *pThis,
	UINT VertexCount,
	UINT StartVertexLocation
	) {
	oDraw(pThis, VertexCount, StartVertexLocation);
	if (pCtxLinearizeBuffer == pThis) {
		pCtxLinearizeBuffer = nullptr;

		ComPtr<ID3D11Device> pDevice;
		pThis->GetDevice(pDevice.GetAddressOf());		

		ComPtr<ID3D11RenderTargetView> pCurrentRTV;
		LOG_CALL(LL_DBG, pThis->OMGetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		pCurrentRTV->GetDesc(&rtvDesc);
		LOG_CALL(LL_DBG, pDevice->CreateRenderTargetView(pLinearDepthTexture.Get(), &rtvDesc, pCurrentRTV.ReleaseAndGetAddressOf()));
		LOG_CALL(LL_DBG, pThis->OMSetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));

		D3D11_TEXTURE2D_DESC ldtDesc;
		pLinearDepthTexture->GetDesc(&ldtDesc);

		D3D11_VIEWPORT viewport;
		viewport.Width = static_cast<float>(ldtDesc.Width);
		viewport.Height = static_cast<float>(ldtDesc.Height);
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;
		pThis->RSSetViewports(1, &viewport);
		D3D11_RECT rect;
		rect.left = rect.top = 0;
		rect.right = ldtDesc.Width;
		rect.bottom = ldtDesc.Height;
		pThis->RSSetScissorRects(1, &rect);

		LOG_CALL(LL_TRC, oDraw(pThis, VertexCount, StartVertexLocation));
	}
}

//static void Detour_StepAudio(void* rcx) {
//	if (::exportContext) {
//		::exportContext->audioSkipCounter += 1.0f;
//		if (::exportContext->audioSkipCounter >= ::exportContext->audioSkip) {
//			oStepAudio(rcx);
//		}
//		while (::exportContext->audioSkipCounter >= ::exportContext->audioSkip) {
//			::exportContext->audioSkipCounter -= ::exportContext->audioSkip;
//		}
//	} else {
//		oStepAudio(rcx);
//	}
//	static int x = 0;
//	if (x == 0) {
//	}
//	x = ++x % 2;
//}

static HANDLE Detour_CreateThread(void* pFunc, void* pParams, int32_t r8d, int32_t r9d, void* rsp20, int32_t rsp28, char* name) {
	void* result = oCreateThread(pFunc, pParams, r8d, r9d, rsp20, rsp28, name);
	LOG(LL_TRC, "CreateThread:",
		" pFunc:", pFunc,
		" pParams:", pParams,
		" r8d:", Logger::hex(r8d, 4),
		" r9d:", Logger::hex(r9d, 4),
		" rsp20:", rsp20,
		" rsp28:", rsp28,
		" name:", name ? name : "<NULL>");

	//if (name) {
	//	if (std::string("RageAudioMixThread").compare(name) == 0) {
	//		threadIdRageAudioMixThread = ::GetThreadId(result);
	//	}
	//}

	return result;
}

//static float Detour_GetGameSpeedMultiplier(void* rcx) {
//	float result = oGetGameSpeedMultiplier(rcx);
//	//if (exportContext) {
//		std::pair<int32_t, int32_t> fps = config::fps;
//		result = 60.0f * (float)fps.second / ((float)fps.first * ((float)config::motion_blur_samples + 1));
//	//}
//	return result;
//}

static float Detour_GetRenderTimeBase(int64_t choice) {
	std::pair<int32_t, int32_t> fps = config::fps;
	float result = 1000.0f * (float)fps.second / ((float)fps.first * ((float)config::motion_blur_samples + 1));
	//float result = 1000.0f / 60.0f;
	LOG(LL_NFO, "Time step: ", result);
	return result;
}

//static void Detour_WaitForSingleObject(void* rcx, int32_t edx) {
//	oWaitForSingleObject(rcx, edx);
//	while (threadIdRageAudioMixThread && (threadIdRageAudioMixThread == ::GetCurrentThreadId())) {
//		static int x = 3;
//		x++;
//		x %= 4;
//		ReleaseSemaphore(rcx, 1, NULL);
//		oWaitForSingleObject(rcx, edx);
//		if (x == 0) {
//			break;
//		}
//		Sleep(0);
//	}
//}

//static uint8_t Detour_AudioUnk01(void* rcx) {
//	return 1;
//}

static void* Detour_CreateTexture(void* rcx, char* name, uint32_t r8d, uint32_t width, uint32_t height, uint32_t format, void* rsp30) {
	void* result = oCreateTexture(rcx, name, r8d, width, height, format, rsp30);

	void** vresult = (void**)result;

	

	ComPtr<ID3D11Texture2D> pTexture;
	DXGI_FORMAT fmt = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
	if ((name != NULL) && (result != NULL)) {
		IUnknown* pUnknown = (IUnknown*)(*(vresult + 7));
		if (pUnknown && SUCCEEDED(pUnknown->QueryInterface(pTexture.GetAddressOf()))) {
			D3D11_TEXTURE2D_DESC desc;
			pTexture->GetDesc(&desc);
			fmt = desc.Format;
		}
	}

	LOG(LL_TRC, "CreateTexture:",
		" rcx:", rcx,
		" name:", name ? name : "<NULL>",
		" r8d:", Logger::hex(r8d, 8),
		" width:", width,
		" height:", height,
		" fmt:", conv_dxgi_format_to_string(fmt),
		" result:", result,
		" *r+0:", vresult ? *(vresult + 0) : "<NULL>",
		" *r+1:", vresult ? *(vresult + 1) : "<NULL>",
		" *r+2:", vresult ? *(vresult + 2) : "<NULL>",
		" *r+3:", vresult ? *(vresult + 3) : "<NULL>",
		" *r+4:", vresult ? *(vresult + 4) : "<NULL>",
		" *r+5:", vresult ? *(vresult + 5) : "<NULL>",
		" *r+6:", vresult ? *(vresult + 6) : "<NULL>",
		" *r+7:", vresult ? *(vresult + 7) : "<NULL>",
		" *r+8:", vresult ? *(vresult + 8) : "<NULL>",
		" *r+9:", vresult ? *(vresult + 9) : "<NULL>",
		" *r+10:", vresult ? *(vresult + 10) : "<NULL>",
		" *r+11:", vresult ? *(vresult + 11) : "<NULL>",
		" *r+12:", vresult ? *(vresult + 12) : "<NULL>",
		" *r+13:", vresult ? *(vresult + 13) : "<NULL>",
		" *r+14:", vresult ? *(vresult + 14) : "<NULL>",
		" *r+15:", vresult ? *(vresult + 15) : "<NULL>");

	if (pTexture) {
		ComPtr<ID3D11Device> pDevice;
		pTexture->GetDevice(pDevice.GetAddressOf());
		if ((std::string("DepthBuffer_Resolved").compare(name) == 0) || (std::string("DepthBufferCopy").compare(name) == 0)) {
			pGameDepthBufferResolved = pTexture;
		} else if (std::string("DepthBuffer").compare(name) == 0) {
			pGameDepthBuffer = pTexture;
		} else if (std::string("Depth Quarter").compare(name) == 0) {
			pGameDepthBufferQuarter = pTexture;
		} else if (std::string("GBUFFER_0").compare(name) == 0) {
			pGameGBuffer0 = pTexture;
		} else if (std::string("Edge Copy").compare(name) == 0) {
			pGameEdgeCopy = pTexture;
			D3D11_TEXTURE2D_DESC desc;
			pTexture->GetDesc(&desc);
			pDevice->CreateTexture2D(&desc, NULL, pStencilTexture.ReleaseAndGetAddressOf());
		} else if (std::string("Depth Quarter Linear").compare(name) == 0) {
			pGameDepthBufferQuarterLinear = pTexture;
			D3D11_TEXTURE2D_DESC desc, resolvedDesc;

			if (!pGameDepthBufferResolved) {
				pGameDepthBufferResolved = pGameDepthBuffer;
			}

			pGameDepthBufferResolved->GetDesc(&resolvedDesc);
			resolvedDesc.ArraySize = 1;
			resolvedDesc.SampleDesc.Count = 1;
			resolvedDesc.SampleDesc.Quality = 0;
			pGameDepthBufferQuarterLinear->GetDesc(&desc);
			desc.Width = resolvedDesc.Width;
			desc.Height = resolvedDesc.Height;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.CPUAccessFlags = 0;
			LOG_CALL(LL_DBG, pDevice->CreateTexture2D(&desc, NULL, pLinearDepthTexture.ReleaseAndGetAddressOf()));
		} else if (std::string("BackBuffer").compare(name) == 0) {
			pGameBackBufferResolved.Reset();
			pGameDepthBuffer.Reset();
			pGameDepthBufferQuarter.Reset();
			pGameDepthBufferQuarterLinear.Reset();
			pGameDepthBufferResolved.Reset();
			pGameEdgeCopy.Reset();
			pGameGBuffer0.Reset();

			D3D11_TEXTURE2D_DESC desc;
			pGameBackBuffer = pTexture;
			pGameBackBuffer->GetDesc(&desc);

			D3D11_TEXTURE2D_DESC accBufDesc = desc;
			accBufDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			accBufDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			accBufDesc.CPUAccessFlags = 0;
			accBufDesc.MiscFlags = 0;
			accBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
			accBufDesc.MipLevels = 1;
			accBufDesc.ArraySize = 1;
			accBufDesc.SampleDesc.Count = 1;
			accBufDesc.SampleDesc.Quality = 0;

			LOG_IF_FAILED(pDevice->CreateTexture2D(&accBufDesc, NULL, pMotionBlurAccBuffer.ReleaseAndGetAddressOf()), "Failed to create accumulation buffer texture");

			D3D11_TEXTURE2D_DESC mbBufferDesc = accBufDesc;
			mbBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
			mbBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

			LOG_IF_FAILED(pDevice->CreateTexture2D(&mbBufferDesc, NULL, pMotionBlurFinalBuffer.ReleaseAndGetAddressOf()), "Failed to create motion blur buffer texture");

			D3D11_RENDER_TARGET_VIEW_DESC accBufRTVDesc;
			accBufRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			accBufRTVDesc.Texture2D.MipSlice = 0;
			accBufRTVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

			
			LOG_IF_FAILED(pDevice->CreateRenderTargetView(pMotionBlurAccBuffer.Get(), &accBufRTVDesc, pRTVAccBuffer.ReleaseAndGetAddressOf()), "Failed to create acc buffer RTV.");

			D3D11_RENDER_TARGET_VIEW_DESC mbBufferRTVDesc;
			mbBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			mbBufferRTVDesc.Texture2D.MipSlice = 0;
			mbBufferRTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

			LOG_IF_FAILED(pDevice->CreateRenderTargetView(pMotionBlurFinalBuffer.Get(), &mbBufferRTVDesc, pRTVBlurBuffer.ReleaseAndGetAddressOf()), "Failed to create blur buffer RTV.");

			D3D11_SHADER_RESOURCE_VIEW_DESC accBufSRVDesc;
			accBufSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			accBufSRVDesc.Texture2D.MipLevels = 1;
			accBufSRVDesc.Texture2D.MostDetailedMip = 0;
			accBufSRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

			LOG_IF_FAILED(pDevice->CreateShaderResourceView(pMotionBlurAccBuffer.Get(), &accBufSRVDesc, pSRVAccBuffer.ReleaseAndGetAddressOf()), "Failed to create blur buffer SRV.");
			
			D3D11_TEXTURE2D_DESC sourceSRVTextureDesc = desc;
			sourceSRVTextureDesc.CPUAccessFlags = 0;
			sourceSRVTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			sourceSRVTextureDesc.MiscFlags = 0;
			sourceSRVTextureDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
			sourceSRVTextureDesc.ArraySize = 1;
			sourceSRVTextureDesc.SampleDesc.Count = 1;
			sourceSRVTextureDesc.SampleDesc.Quality = 0;
			sourceSRVTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			sourceSRVTextureDesc.MipLevels = 1;

			REQUIRE(pDevice->CreateTexture2D(&sourceSRVTextureDesc, NULL, pSourceSRVTexture.ReleaseAndGetAddressOf()), "Failed to create back buffer copy texture");

			D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
			sourceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			sourceSRVDesc.Texture2D.MipLevels = 1;
			sourceSRVDesc.Texture2D.MostDetailedMip = 0;
			sourceSRVDesc.Format = sourceSRVTextureDesc.Format;

			LOG_IF_FAILED(pDevice->CreateShaderResourceView(pSourceSRVTexture.Get(), &sourceSRVDesc, pSourceSRV.ReleaseAndGetAddressOf()), "Failed to create backbuffer copy SRV.");

			/*ComPtr<ID3D11DeviceContext> pDeviceContext;
			pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());*/
		} else if ((std::string("BackBuffer_Resolved").compare(name) == 0) || (std::string("BackBufferCopy").compare(name) == 0)) {
			pGameBackBufferResolved = pTexture;
		} else if (std::string("VideoEncode").compare(name) == 0) {
			ComPtr<ID3D11Texture2D> pExportTexture;
			pExportTexture = pTexture;

			D3D11_TEXTURE2D_DESC desc;
			pExportTexture->GetDesc(&desc);

			LOG(LL_NFO, "Detour_CreateTexture: fmt:", conv_dxgi_format_to_string(desc.Format),
				" w:", desc.Width,
				" h:", desc.Height);
			std::lock_guard<std::mutex> sessionLock(mxSession);
			LOG_CALL(LL_DBG, ::exportContext.reset());
			LOG_CALL(LL_DBG, session.reset());
			try {
				LOG(LL_NFO, "Creating session...");

				if (config::auto_reload_config) {
					LOG_CALL(LL_DBG, config::reload());
				}

				session.reset(new Encoder::Session());
				NOT_NULL(session, "Could not create the session");
				::exportContext.reset(new ExportContext());
				NOT_NULL(::exportContext, "Could not create export context");
				::exportContext->pSwapChain = mainSwapChain;
				::exportContext->pExportRenderTarget = pExportTexture;

				
				pExportTexture->GetDevice(::exportContext->pDevice.GetAddressOf());
				::exportContext->pDevice->GetImmediateContext(::exportContext->pDeviceContext.GetAddressOf());
			} catch (std::exception& ex) {
				LOG(LL_ERR, ex.what());
				LOG_CALL(LL_DBG, session.reset());
				LOG_CALL(LL_DBG, ::exportContext.reset());
			}
			//}
		}
	}

	return result;
}

static void prepareDeferredContext(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext) {
	REQUIRE(pDevice->CreateDeferredContext(0, pDContext.GetAddressOf()), "Failed to create deferred context"); 
	REQUIRE(pDevice->CreateVertexShader(g_VSFullScreen, sizeof(g_VSFullScreen), NULL, pVSFullScreen.GetAddressOf()), "Failed to create g_VSFullScreen vertex shader");
	REQUIRE(pDevice->CreatePixelShader(g_PSAccumulate, sizeof(g_PSAccumulate), NULL, pPSAccumulate.GetAddressOf()), "Failed to create pixel shader");
	REQUIRE(pDevice->CreatePixelShader(g_PSDivide, sizeof(g_PSDivide), NULL, pPSDivide.GetAddressOf()), "Failed to create pixel shader");

	D3D11_BUFFER_DESC clipBufDesc;
	clipBufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER; 
	clipBufDesc.ByteWidth = sizeof(PSConstantBuffer); 
	clipBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE; 
	clipBufDesc.MiscFlags = 0; 
	clipBufDesc.StructureByteStride = 0; 
	clipBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC; 

	REQUIRE(pDevice->CreateBuffer(&clipBufDesc, NULL, pDivideConstantBuffer.GetAddressOf()), "Failed to create constant buffer for pixel shader"); 
	
	D3D11_SAMPLER_DESC samplerDesc; 
	samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; 
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP; 
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP; 
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP; 
	samplerDesc.MipLODBias = 0.0f; 
	samplerDesc.MaxAnisotropy = 1; 
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER; 
	samplerDesc.BorderColor[0] = 1.0f; 
	samplerDesc.BorderColor[1] = 1.0f; 
	samplerDesc.BorderColor[2] = 1.0f; 
	samplerDesc.BorderColor[3] = 1.0f; 
	samplerDesc.MinLOD = -FLT_MAX; 
	samplerDesc.MaxLOD = FLT_MAX;

	REQUIRE(pDevice->CreateSamplerState(&samplerDesc, pPSSampler.GetAddressOf()), "Failed to create sampler state"); 

	D3D11_RASTERIZER_DESC rasterStateDesc; 
	rasterStateDesc.AntialiasedLineEnable = FALSE; 
	rasterStateDesc.CullMode = D3D11_CULL_FRONT;
	rasterStateDesc.DepthClipEnable = FALSE; 
	rasterStateDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID; 
	rasterStateDesc.MultisampleEnable = FALSE; 
	rasterStateDesc.ScissorEnable = FALSE;
	REQUIRE(pDevice->CreateRasterizerState(&rasterStateDesc, pRasterState.GetAddressOf()), "Failed to create raster state");


	D3D11_BLEND_DESC blendDesc;
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	
	REQUIRE(pDevice->CreateBlendState(&blendDesc, pAccBlendState.GetAddressOf()), "Failed to create accumulation blend state");
}

static void drawAdditive(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Texture2D> pSource) {
	D3D11_TEXTURE2D_DESC desc;
	pSource->GetDesc(&desc);
	
	pDContext->ClearState();
	pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0);
	pDContext->IASetInputLayout(NULL);
	pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pDContext->VSSetShader(pVSFullScreen.Get(), NULL, 0);
	pDContext->PSSetSamplers(0, 1, pPSSampler.GetAddressOf());
	pDContext->PSSetShader(pPSAccumulate.Get(), NULL, 0);
	pDContext->RSSetState(pRasterState.Get());

	float factors[4] = { 0,0,0,0 };

	pDContext->OMSetBlendState(pAccBlendState.Get(), factors, 0xFFFFFFFF);

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = desc.Width;
	viewport.Height = desc.Height;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
	pDContext->RSSetViewports(1, &viewport);

	pDContext->CopyResource(pSourceSRVTexture.Get(), pSource.Get());

	pDContext->PSSetShaderResources(0, 1, pSourceSRV.GetAddressOf());
	pDContext->OMSetRenderTargets(1, pRTVAccBuffer.GetAddressOf(), nullptr);
	if (::exportContext->accCount == 0) {
		float color[4] = { 0, 0, 0, 1 };
		pDContext->ClearRenderTargetView(pRTVAccBuffer.Get(), color);
	}

	pDContext->Draw(4, 0);

	ComPtr<ID3D11CommandList> pCmdList;
	pDContext->FinishCommandList(FALSE, pCmdList.GetAddressOf());
	pContext->ExecuteCommandList(pCmdList.Get(), TRUE);

	::exportContext->accCount++;
}

static ComPtr<ID3D11Texture2D> divideBuffer(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext, uint32_t k) {
	
	D3D11_TEXTURE2D_DESC desc;
	pMotionBlurFinalBuffer->GetDesc(&desc);

	pDContext->ClearState();
	pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0);
	pDContext->IASetInputLayout(NULL);
	pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pDContext->VSSetShader(pVSFullScreen.Get(), NULL, 0);
	pDContext->PSSetShader(pPSDivide.Get(), NULL, 0);

	D3D11_MAPPED_SUBRESOURCE mapped;
	REQUIRE(pDContext->Map(pDivideConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Failed to map divide shader constant buffer");;
	float* floats = (float*)mapped.pData;
	floats[0] = static_cast<float>(k);
	/*floats[0] = 1.0f;*/
	pDContext->Unmap(pDivideConstantBuffer.Get(), 0);

	pDContext->PSSetConstantBuffers(0, 1, pDivideConstantBuffer.GetAddressOf());
	pDContext->PSSetShaderResources(0, 1, pSRVAccBuffer.GetAddressOf());
	pDContext->RSSetState(pRasterState.Get());

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = desc.Width;
	viewport.Height = desc.Height;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
	pDContext->RSSetViewports(1, &viewport);
	

	pDContext->OMSetRenderTargets(1, pRTVBlurBuffer.GetAddressOf(), NULL);
	float color[4] = { 0, 0, 0, 1 };
	pDContext->ClearRenderTargetView(pRTVBlurBuffer.Get(), color);

	pDContext->Draw(4, 0);

	ComPtr<ID3D11CommandList> pCmdList;
	pDContext->FinishCommandList(FALSE, pCmdList.GetAddressOf());
	pContext->ExecuteCommandList(pCmdList.Get(), TRUE);

	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;

	ComPtr<ID3D11Texture2D> ret;
	REQUIRE(pDevice->CreateTexture2D(&desc, NULL, ret.GetAddressOf()), "Failed to create copy of motion blur dest texture");
	pContext->CopyResource(ret.Get(), pMotionBlurFinalBuffer.Get());

	return ret;

}