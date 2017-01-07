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
#include "VSPassThrough.h"
#include "PSLinearizeDepth.h"

#include "..\DirectXTex\DirectXTex\DirectXTex.h"
#include "hook-def.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace {
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessInput(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessMessage(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_AddStream(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_SetInputMediaType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_WriteSample(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_Finalize(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkOMSetRenderTargets(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateRenderTargetView(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateDepthStencilView(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateTexture2D(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkRSSetViewports(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkRSSetScissorRects(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkVSSetConstantBuffers(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkClearState(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkDispatch(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkDispatchIndirect(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkDraw(new PLH::VFuncDetour);
	
	std::shared_ptr<PLH::IATHook> hkCoCreateInstance(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateSinkWriterFromURL(new PLH::IATHook);

	std::shared_ptr<PLH::X64Detour> hkGetFrameRateFraction(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetRenderTimeBase(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetFrameRate(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetAudioSamples(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkUnk01(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateTexture(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateExportTexture(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkLinearizeTexture(new PLH::X64Detour);

	/*std::shared_ptr<PLH::X64Detour> hkGetGlobalVariableIndex(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVariable(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetMatrices(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVar(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVarPtrByHash(new PLH::X64Detour);*/
	ComPtr<ID3D11Texture2D> pGameDepthBufferResolved;
	ComPtr<ID3D11Texture2D> pGameBackBuffer;
	ComPtr<ID3D11Texture2D> pGameBackBufferResolved;
	ComPtr<ID3D11Texture2D> pGameDepthBufferQuarterLinear;
	ComPtr<ID3D11Texture2D> pGameDepthBufferQuarter;
	ComPtr<ID3D11Texture2D> pLinearDepthTexture;
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
tCreateTexture2D oCreateTexture2D;
tCreateRenderTargetView oCreateRenderTargetView;
tCreateDepthStencilView oCreateDepthStencilView;
tRSSetViewports oRSSetViewports;
tRSSetScissorRects oRSSetScissorRects;
tGetRenderTimeBase oGetRenderTimeBase;
tCreateTexture oCreateTexture;
tVSSetConstantBuffers oVSSetConstantBuffers;
tClearState oClearState;
tLinearizeTexture oLinearizeTexture;
tDispatch oDispatch;
tDispatchIndirect oDispatchIndirect;
tDraw oDraw;
//tGetGlobalVariableIndex oGetGlobalVariableIndex;
//tGetVariable oGetVariable;
//tGetMatrices oGetMatrices;
//tGetVarHash oGetVarHash;
//tGetVarPtrByHash oGetVarPtrByHash;
//tGetVarPtrByHash2 oGetVarPtrByHash2;
//tGetTexture oGetTexture;
//tGetFrameRate oGetFrameRate;
//tGetAudioSamples oGetAudioSamples;
//tgetFrameRateFraction ogetFrameRateFraction;
//tUnk01 oUnk01;

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
			REQUIRE(hookVirtualFunction(pDevice.Get(), 5, &Hook_CreateTexture2D, &oCreateTexture2D, hkCreateTexture2D), "Failed to hook ID3DDeviceContext::Map");
			REQUIRE(hookVirtualFunction(pDevice.Get(), 9, &Hook_CreateRenderTargetView, &oCreateRenderTargetView, hkCreateRenderTargetView), "Failed to hook ID3DDeviceContext::CreateRenderTargetView");
			REQUIRE(hookVirtualFunction(pDevice.Get(), 10, &Hook_CreateDepthStencilView, &oCreateDepthStencilView, hkCreateDepthStencilView), "Failed to hook ID3DDeviceContext::CreateDepthStencilView");
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 7, &VSSetConstantBuffers, &oVSSetConstantBuffers, hkVSSetConstantBuffers), "Failed to hook ID3DDeviceContext::VSSetConstantBuffers");

			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 13, &Draw, &oDraw, hkDraw), "Failed to hook ID3DDeviceContext::Draw");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 33, &Hook_OMSetRenderTargets, &oOMSetRenderTargets, hkOMSetRenderTargets), "Failed to hook ID3DDeviceContext::OMSetRenderTargets");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 41, &Dispatch, &oDispatch, hkDispatch), "Failed to hook ID3DDeviceContext::Dispatch");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 42, &DispatchIndirect, &oDispatchIndirect, hkDispatchIndirect), "Failed to hook ID3DDeviceContext::DispatchIndirect");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 44, &RSSetViewports, &oRSSetViewports, hkRSSetViewports), "Failed to hook ID3DDeviceContext::RSSetViewports"); 
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 45, &RSSetScissorRects, &oRSSetScissorRects, hkRSSetScissorRects), "Failed to hook ID3DDeviceContext::RSSetViewports"); 
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 110, &ClearState, &oClearState, hkClearState), "Failed to hook ID3DDeviceContext::ClearState");

			ComPtr<IDXGIDevice> pDXGIDevice;
			REQUIRE(pDevice.As(&pDXGIDevice), "Failed to get IDXGIDevice from ID3D11Device");
			
			ComPtr<IDXGIAdapter> pDXGIAdapter;
			REQUIRE(pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)pDXGIAdapter.GetAddressOf()), "Failed to get IDXGIAdapter");

			REQUIRE(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)pDXGIFactory.GetAddressOf()), "Failed to get IDXGIFactory");

		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}

	/*uint32_t hash = Detour_GetVarHash("World", 0);
	LOG(LL_DBG, "0x", Logger::hex(hash, 16));
	if (hash) {*/
		/*void* ptr = Detour_GetVarPtrByHash2("gClipPlanes");
		LOG(LL_DBG, ptr);
		if (ptr) {
			LOG(LL_DBG, "gClipPlanes: ", *(float*)ptr);
		}*/
	//}
}

void initialize() {
	PRE();
	try {
		mainThreadId = std::this_thread::get_id();

		REQUIRE(hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL, &oMFCreateSinkWriterFromURL, hkMFCreateSinkWriterFromURL), "Failed to hook MFCreateSinkWriterFromURL in mfreadwrite.dll");
		REQUIRE(hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance, hkCoCreateInstance), "Failed to hook CoCreateInstance in ole32.dll");
			
		pYaraHelper.reset(new YaraHelper());
		pYaraHelper->initialize();

		MODULEINFO info;
		GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &info, sizeof(info));
		LOG(LL_NFO, "Image base:", ((void*)info.lpBaseOfDll));

		void* pGetRenderTimeBase = NULL;
		//void* pCreateExportTexture = NULL;
		void* pCreateTexture = NULL;
		void* pLinearizeTexture = NULL;
		pYaraHelper->addEntry("yara_get_render_time_base_function", yara_get_render_time_base_function, &pGetRenderTimeBase);
		//pYaraHelper->addEntry("yara_create_export_texture_function", yara_create_export_texture_function, &pCreateExportTexture);
		pYaraHelper->addEntry("yara_create_texture_function", yara_create_texture_function, &pCreateTexture);
		pYaraHelper->addEntry("yara_linearize_texture_function", yara_linearize_texture_function, &pLinearizeTexture);
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

			if (pLinearizeTexture) {
				REQUIRE(hookX64Function(pLinearizeTexture, &Detour_LinearizeTexture, &oLinearizeTexture, hkLinearizeTexture), "Failed to hook CreateTexture function.");
			} else {
				LOG(LL_ERR, "Could not find the address for LinearizeTexture function.");
			}

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
		//for (int cam = 0; cam < 100; cam++) {
		//	//Cam cam = CAM::GET_RENDERING_CAM();
		//	if (CAM::GET_CAM_NEAR_CLIP(cam) != 0 || CAM::GET_CAM_FAR_CLIP(cam) != 0) {
		//		LOG(LL_DBG, cam, " ", CAM::GET_CAM_NEAR_CLIP(cam), " ", CAM::GET_CAM_FAR_CLIP(cam));
		//	}
		//}
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
	char buffer[64];
	GUIDToString(rclsid, buffer, 64);
	LOG(LL_NFO, "CoCreateInstance: ", buffer);
	POST();
	return result;
}

static void ClearState(
	ID3D11DeviceContext *pThis
	) {
	ComPtr<ID3D11RenderTargetView> pOldRTV;
	ComPtr<ID3D11Resource> pOldRTVTexture;
	pThis->OMGetRenderTargets(1, pOldRTV.GetAddressOf(), NULL);
	if (pOldRTV) {
		pOldRTV->GetResource(pOldRTVTexture.GetAddressOf());
	}

	if (pOldRTVTexture && (pOldRTVTexture == pGameDepthBufferQuarterLinear)) {
		LOG(LL_DBG, "Hello");
	}
}

static HRESULT Hook_CreateRenderTargetView(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView        **ppRTView
	) {
	//if ((exportContext != NULL) && exportContext->captureRenderTargetViewReference && (std::this_thread::get_id() == mainThreadId)) {
	//	try {
	//		LOG(LL_NFO, "Capturing export render target view...");
	//		exportContext->captureRenderTargetViewReference = false;
	//		exportContext->pDevice = pThis;
	//		exportContext->pDevice->GetImmediateContext(exportContext->pDeviceContext.GetAddressOf());

	//		ComPtr<ID3D11Texture2D> pTexture;
	//		REQUIRE(pResource->QueryInterface(pTexture.GetAddressOf()), "Failed to convert ID3D11Resource to ID3D11Texture2D");
	//		D3D11_TEXTURE2D_DESC desc;
	//		pTexture->GetDesc(&desc);

	//		/*DXGI_SWAP_CHAIN_DESC swapChainDesc;
	//		mainSwapChain->GetDesc(&swapChainDesc);*/

	//		/*desc.Width = swapChainDesc.BufferDesc.Width;
	//		desc.Height = swapChainDesc.BufferDesc.Height;*/

	//		LOG(LL_DBG, "Creating render target view: w:", desc.Width, " h:", desc.Height);

	//		/*ComPtr<ID3D11Texture2D> pTexture;
	//		pThis->CreateTexture2D(&desc, NULL, pTexture.GetAddressOf());
	//		ComPtr<ID3D11Resource> pRes;
	//		REQUIRE(pTexture.As(&pRes), "Failed to convert ID3D11Texture2D to ID3D11Resource");*/
	//		HRESULT result = oCreateRenderTargetView(pThis, pResource, pDesc, ppRTView);
	//		exportContext->pExportRenderTarget = pTexture;
	//		exportContext->pExportRenderTargetView = *(ppRTView);
	//		POST();
	//		return result;
	//	} catch (std::exception& ex) {
	//		LOG(LL_ERR, ex.what());
	//	}
	//}
	return oCreateRenderTargetView(pThis, pResource, pDesc, ppRTView);
}

static HRESULT Hook_CreateDepthStencilView(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	ID3D11DepthStencilView        **ppDepthStencilView
	) {
	HRESULT result = oCreateDepthStencilView(pThis, pResource, pDesc, ppDepthStencilView);
	return result;
}

static void RSSetViewports(
	ID3D11DeviceContext  *pThis,
	UINT                 NumViewports,
	const D3D11_VIEWPORT *pViewports
	) {
	/*if ((exportContext != NULL) && isCurrentRenderTargetView(pThis, exportContext->pExportRenderTargetView)) {
		D3D11_VIEWPORT* pNewViewports = new D3D11_VIEWPORT[NumViewports];
		for (UINT i = 0; i < NumViewports; i++) {
			pNewViewports[i] = pViewports[i];
			DXGI_SWAP_CHAIN_DESC desc;
			exportContext->pSwapChain->GetDesc(&desc);
			pNewViewports[i].Width = static_cast<float>(desc.BufferDesc.Width);
			pNewViewports[i].Height = static_cast<float>(desc.BufferDesc.Height);
		}
		return oRSSetViewports(pThis, NumViewports, pNewViewports);
	}*/

	return oRSSetViewports(pThis, NumViewports, pViewports);
}

static void RSSetScissorRects(
	ID3D11DeviceContext *pThis,
	UINT          NumRects,
	const D3D11_RECT *  pRects
	) {
	/*if ((exportContext != NULL) && isCurrentRenderTargetView(pThis, exportContext->pExportRenderTargetView)) {
		D3D11_RECT* pNewRects = new D3D11_RECT[NumRects];
		for (UINT i = 0; i < NumRects; i++) {
			pNewRects[i] = pRects[i];
			DXGI_SWAP_CHAIN_DESC desc;
			exportContext->pSwapChain->GetDesc(&desc);
			pNewRects[i].right = desc.BufferDesc.Width;
			pNewRects[i].bottom = desc.BufferDesc.Height;
		}
		return oRSSetScissorRects(pThis, NumRects, pNewRects);
	}*/

	return oRSSetScissorRects(pThis, NumRects, pRects);
}

static void Hook_OMSetRenderTargets(
	ID3D11DeviceContext           *pThis,
	UINT                          NumViews,
	ID3D11RenderTargetView *const *ppRenderTargetViews,
	ID3D11DepthStencilView        *pDepthStencilView
	) {

	for (int i = 0; i < NumViews; i++) {
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
	/*if (pDepthStencilView) {
		ComPtr<ID3D11Resource> pDepthTexture;
		pDepthStencilView->GetResource(pDepthTexture.GetAddressOf());
		if (pDepthTexture == pGameDepthBufferQuarterLinear) {
			LOG(LL_DBG, "FUCK!!!");
		}
	}*/

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

	if (::exportContext && pOldRTVTexture && (pOldRTVTexture == pGameBackBuffer)) {
		ID3D11Buffer* pBufferArray[2];
		pThis->VSGetConstantBuffers(0, 2, &pBufferArray[0]);
		if (pBufferArray[1]) {
			D3D11_BUFFER_DESC bufferDesc;
			pBufferArray[1]->GetDesc(&bufferDesc);
			bufferDesc.BindFlags = 0;
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
			bufferDesc.MiscFlags = 0;
			bufferDesc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;
			ComPtr<ID3D11Buffer> pBufferCopy;
			::exportContext->pDevice->CreateBuffer(&bufferDesc, NULL, pBufferCopy.GetAddressOf());
			pThis->CopyResource(pBufferCopy.Get(), pBufferArray[1]);

			D3D11_MAPPED_SUBRESOURCE res;
			if (SUCCEEDED(pThis->Map(pBufferCopy.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &res))) {
				DirectX::XMMATRIX matrix = DirectX::XMMATRIX((float*)res.pData + 32);
				DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, matrix);
				DirectX::XMVECTOR far_local = { 0, 0, 1, 1 };
				DirectX::XMVECTOR near_local = { 0, 0, 0, 1 };

				DirectX::XMVECTOR far_global = DirectX::XMVector4Transform(far_local, inverse);
				DirectX::XMVECTOR near_global = DirectX::XMVector4Transform(near_local, inverse);

				::exportContext->far_clip = DirectX::XMVectorGetByIndex(DirectX::XMVector4Length(far_global), 0);
				::exportContext->near_clip = DirectX::XMVectorGetByIndex(DirectX::XMVector4Length(near_global), 0);
				LOG(LL_DBG, " near:", ::exportContext->near_clip, " far:", ::exportContext->far_clip);
			}
		}
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

				if (config::export_openexr) {
					{
						D3D11_TEXTURE2D_DESC desc;
						pLinearDepthTexture->GetDesc(&desc);

						//ComPtr<ID3D11DeviceContext> pDContext;
						//REQUIRE(pDevice->CreateDeferredContext(0, pDContext.GetAddressOf()), "Failed to create deferred context");

						//pDContext->ClearState();

						//ComPtr<ID3D11Texture2D> pDepthBufferLinear;
						//D3D11_TEXTURE2D_DESC depthBufLinDesc = desc;
						//depthBufLinDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
						///*depthBufLinDesc.Format = desc.Format;
						//depthBufLinDesc.CPUAccessFlags = 0;
						//depthBufLinDesc.MiscFlags = 0;*/
						//depthBufLinDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
						///*depthBufLinDesc.MipLevels = 1;
						//depthBufLinDesc.ArraySize = 1;*/

						//REQUIRE(pDevice->CreateTexture2D(&depthBufLinDesc, NULL, pDepthBufferLinear.GetAddressOf()), "Failed to create linear depth buffer texture");
						//ComPtr<ID3D11Texture2D> pSRVTexture;
						//REQUIRE(pDevice->CreateTexture2D(&depthBufLinDesc, NULL, pSRVTexture.GetAddressOf()), "Failed to create shader resource view texture");
						//pThis->CopyResource(pSRVTexture.Get(), pGameDepthBufferResolved.Get());

						//ComPtr<ID3D11VertexShader> pVS;
						//REQUIRE(pDevice->CreateVertexShader(g_VSPassThrough, sizeof(g_VSPassThrough), NULL, pVS.GetAddressOf()), "Failed to create vertex shader");

						//ComPtr<ID3D11PixelShader> pPS;
						//REQUIRE(pDevice->CreatePixelShader(g_PSLinearizeDepth, sizeof(g_PSLinearizeDepth), NULL, pPS.GetAddressOf()), "Failed to create pixel shader");

						///*pDContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
						//pDContext->IASetIndexBuffer(NULL, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0);
						//pDContext->IASetInputLayout(NULL);
						//D3D11_PRIMITIVE_TOPOLOGY topoCache;
						//pThis->IAGetPrimitiveTopology(&topoCache);
						//LOG(LL_DBG, "Primitive Topology: ", topoCache);
						//pDContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);*/
						////pDContext->IASetInputLayout(NULL);

						//pDContext->VSSetShader(pVS.Get(), NULL, 0);

						//pDContext->PSSetShader(pPS.Get(), NULL, 0);

						//struct ClippingPlane {
						//	XMFLOAT4 planes;
						//} cbClippingPlanes;
						//cbClippingPlanes.planes.x = ::exportContext->near_clip;
						//cbClippingPlanes.planes.y = ::exportContext->far_clip;

						//ComPtr<ID3D11Buffer> pClippingPlanes;
						//D3D11_BUFFER_DESC clipBufDesc;
						//clipBufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
						//clipBufDesc.ByteWidth = sizeof(ClippingPlane);
						//clipBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
						//clipBufDesc.MiscFlags = 0;
						//clipBufDesc.StructureByteStride = 0;
						//clipBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;

						//D3D11_SUBRESOURCE_DATA clipData = { 0 };
						//clipData.pSysMem = &cbClippingPlanes;

						//REQUIRE(pDevice->CreateBuffer(&clipBufDesc, &clipData, pClippingPlanes.GetAddressOf()), "Failed to create constant buffer for pixel shader");
						//pDContext->PSSetConstantBuffers(0, 1, pClippingPlanes.GetAddressOf());

						//ComPtr<ID3D11DepthStencilView> pDSV;
						//D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
						//dsv_desc.Flags = 0;
						//dsv_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
						//dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
						//dsv_desc.Texture2D.MipSlice = 0;
						//REQUIRE(pDevice->CreateDepthStencilView(pDepthBufferLinear.Get(), &dsv_desc, pDSV.GetAddressOf()), "Failed to create depth stencil view");
						//
						//ComPtr<ID3D11ShaderResourceView> pSRV;
						//D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
						//ZeroMemory(&srvDesc, sizeof(srvDesc));
						//srvDesc.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D;
						//srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
						//srvDesc.Texture2D.MipLevels = 1;
						//srvDesc.Texture2D.MostDetailedMip = 0;
						//REQUIRE(pDevice->CreateShaderResourceView(pSRVTexture.Get(), &srvDesc, pSRV.GetAddressOf()), "Failed to create shader resource view.");
						//pDContext->PSSetShaderResources(0, 1, pSRV.GetAddressOf());

						///*ComPtr<ID3D11Texture2D> pSRVTexture;
						//D3D11_TEXTURE2D_DESC srvBufDesc = desc;
						//srvBufDesc.ArraySize = 1;
						//srvBufDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32G8X24_TYPELESS;
						//srvBufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
						//srvBufDesc.CPUAccessFlags = 0;
						//srvBufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
						//srvBufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
						//
						//REQUIRE(pDevice->CreateTexture2D(&srvBufDesc, NULL, pSRVTexture.GetAddressOf()), "Failed to create shader resource view's texture");

						//pDContext->CopyResource(pSRVTexture.Get(), pGameDepthBufferResolved.Get());*/


						////REQUIRE(pDevice->CreateShaderResourceView(pSRV2Texture.Get(), NULL, pSRV2.GetAddressOf());

						//ComPtr<ID3D11SamplerState> pPSSampler;
						//D3D11_SAMPLER_DESC samplerDesc;
						//samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
						//samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
						//samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
						//samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
						//samplerDesc.MipLODBias = 0.0f;
						//samplerDesc.MaxAnisotropy = 1;
						//samplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER;
						//samplerDesc.BorderColor[0] = 1.0f;
						//samplerDesc.BorderColor[1] = 1.0f;
						//samplerDesc.BorderColor[2] = 1.0f;
						//samplerDesc.BorderColor[3] = 1.0f;
						//samplerDesc.MinLOD = -FLT_MAX;
						//samplerDesc.MaxLOD = FLT_MAX;

						//REQUIRE(pDevice->CreateSamplerState(&samplerDesc, pPSSampler.GetAddressOf()), "Failed to create sampler state");

						//pDContext->PSSetSamplers(0, 1, pPSSampler.GetAddressOf());


						//ComPtr<ID3D11Texture2D> pNullTexture;
						//D3D11_TEXTURE2D_DESC nullTexDesc;
						//nullTexDesc = desc;
						//nullTexDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
						//nullTexDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
						//nullTexDesc.CPUAccessFlags = 0;
						//nullTexDesc.MipLevels = 1;
						//nullTexDesc.MiscFlags = 0;
						//nullTexDesc.Usage = D3D11_USAGE_DEFAULT;
						//nullTexDesc.ArraySize = 1;
						//nullTexDesc.SampleDesc.Count = 1;
						//nullTexDesc.SampleDesc.Quality = 0;
						//REQUIRE(pDevice->CreateTexture2D(&nullTexDesc, NULL, pNullTexture.GetAddressOf()), "Failed to create null back buffer");

						//ComPtr<ID3D11RenderTargetView> pDepthRTV;
						//D3D11_RENDER_TARGET_VIEW_DESC depthRTVDesc;
						//depthRTVDesc.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D;
						//depthRTVDesc.Texture2D.MipSlice = 0;
						//depthRTVDesc.Format = nullTexDesc.Format;

						//REQUIRE(pDevice->CreateRenderTargetView(pNullTexture.Get(), &depthRTVDesc, pDepthRTV.GetAddressOf()), "Failed to create render target view.");

						//pDContext->OMSetRenderTargets(1, pDepthRTV.GetAddressOf(), pDSV.Get());

						//ComPtr<ID3D11RasterizerState> pRasterState;
						//D3D11_RASTERIZER_DESC rasterStateDesc;
						//rasterStateDesc.AntialiasedLineEnable = FALSE;
						//rasterStateDesc.CullMode = D3D11_CULL_NONE;
						//rasterStateDesc.DepthClipEnable = TRUE;
						//rasterStateDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
						//rasterStateDesc.MultisampleEnable = FALSE;
						//rasterStateDesc.ScissorEnable = FALSE;
						//REQUIRE(pDevice->CreateRasterizerState(&rasterStateDesc, pRasterState.GetAddressOf()), "Failed to create raster state");

						//pDContext->RSSetState(pRasterState.Get());

						//ComPtr<ID3D11DepthStencilState> pDSState;
						//D3D11_DEPTH_STENCIL_DESC dsDesc;
						////dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
						//dsDesc.DepthEnable = TRUE;
						//dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
						//dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
						//dsDesc.StencilEnable = FALSE;

						//pDevice->CreateDepthStencilState(&dsDesc, pDSState.GetAddressOf());
						//pDContext->OMSetDepthStencilState(pDSState.Get(), 0);

						///*D3D11_VIEWPORT viewport;
						//viewport.TopLeftX = 0;
						//viewport.TopLeftY = 0;
						//viewport.Height = desc.Height;
						//viewport.Width = desc.Width;
						//viewport.MinDepth = 0;
						//viewport.MaxDepth = 1;
						//pDContext->RSSetViewports(1, &viewport);*/


						//pDContext->Draw(6, 0);

						//ComPtr<ID3D11CommandList> pCmdList;
						//pDContext->FinishCommandList(FALSE, pCmdList.GetAddressOf());
						//pThis->ExecuteCommandList(pCmdList.Get(), TRUE);

						//// It seems primitive topology state is not restored after ID3D11DeviceContext::ExecuteCommandList
						////pThis->IASetPrimitiveTopology(topoCache);

						desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
						desc.BindFlags = 0;
						desc.MiscFlags = 0;
						desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

						REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pDepthBufferCopy.GetAddressOf()), "Failed to create depth buffer copy texture");

						if (pLinearDepthTexture) {
							pThis->CopyResource(pDepthBufferCopy.Get(), pLinearDepthTexture.Get());
						} else {
							LOG(LL_ERR, "Couldn't get linear depth texture");
						}
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
					session->enqueueEXRImage(pThis, pBackBufferCopy, pDepthBufferCopy);
				}
				LOG_CALL(LL_DBG, ::exportContext->pSwapChain->Present(0, DXGI_PRESENT_TEST)); // IMPORTANT: This call makes ENB and ReShade effects to be applied to the render target

				ComPtr<ID3D11Texture2D> pSwapChainBuffer;
				REQUIRE(::exportContext->pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pSwapChainBuffer.GetAddressOf()), "Failed to get swap chain's buffer");
								
				auto& image_ref = *(::exportContext->capturedImage);
				LOG_CALL(LL_DBG, DirectX::CaptureTexture(::exportContext->pDevice.Get(), ::exportContext->pDeviceContext.Get(), pSwapChainBuffer.Get(), image_ref));
				if (::exportContext->capturedImage->GetImageCount() == 0) {
					LOG(LL_ERR, "There is no image to capture.");
					throw std::exception();
				}
				const DirectX::Image* image = ::exportContext->capturedImage->GetImage(0, 0, 0);
				NOT_NULL(image, "Could not get current frame.");
				NOT_NULL(image->pixels, "Could not get current frame.");

				REQUIRE(session->enqueueVideoFrame(image->pixels, (int)(image->width * image->height * 4)), "Failed to enqueue frame");
				::exportContext->capturedImage->Release();
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

				// Create Video Context
				{
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


						if (((float)fps.first * ((float)config::motion_blur_samples + 1) / (float)fps.second) > 60.0f) {
							LOG(LL_NON, "fps * (motion_blur_samples + 1) > 60.0!!!");
							LOG(LL_NON, "Audio export will be disabled!!!");
							::exportContext->isAudioExportDisabled = true;
						}
					}

					REQUIRE(session->createVideoContext(desc.BufferDesc.Width, desc.BufferDesc.Height, "bgra", fps_num, fps_den, config::motion_blur_samples,  config::video_fmt, config::video_enc, config::video_cfg), "Failed to create video context");
				}
				
				// Create Audio Context
				{
					UINT32 blockAlignment, numChannels, sampleRate, bitsPerSample;
					GUID subType;

					pInputMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
					pInputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
					pInputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
					pInputMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
					pInputMediaType->GetGUID(MF_MT_SUBTYPE, &subType);

					if (IsEqualGUID(subType, MFAudioFormat_PCM)) {
						REQUIRE(session->createAudioContext(numChannels, sampleRate, bitsPerSample, AV_SAMPLE_FMT_S16, blockAlignment, config::audio_rate, config::audio_fmt, config::audio_enc, config::audio_cfg), "Failed to create audio context.");
					} else {
						char buffer[64];
						GUIDToString(subType, buffer, 64);
						LOG(LL_ERR, "Unsupported input audio format: ", buffer);
						throw std::runtime_error("Unsupported input audio format");
					}
				}

				// Create Format Context
				{
					char buffer[128];
					std::string output_file = config::output_dir;
					std::string exrOutputPath;

					output_file += "\\XVX-";
					time_t rawtime;
					struct tm timeinfo;
					time(&rawtime);
					localtime_s(&timeinfo, &rawtime);
					strftime(buffer, 128, "%Y%m%d%H%M%S", &timeinfo);
					output_file += buffer;
					exrOutputPath = std::regex_replace(output_file, std::regex("\\\\+"), "\\");
					output_file += "." + config::container_format;

					std::string filename = std::regex_replace(output_file, std::regex("\\\\+"), "\\");

					LOG(LL_NFO, "Output file: ", filename);

					REQUIRE(session->createFormatContext(filename.c_str(), exrOutputPath), "Failed to create format context");
				}
			} catch (std::exception&) {
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
	hkIMFTransform_ProcessInput->UnHook();
	hkIMFTransform_ProcessMessage->UnHook();
	hkIMFSinkWriter_AddStream->UnHook();
	hkIMFSinkWriter_SetInputMediaType->UnHook();
	hkIMFSinkWriter_WriteSample->UnHook();
	hkIMFSinkWriter_Finalize->UnHook();
	POST();
}

static HRESULT Hook_CreateTexture2D(
	ID3D11Device 				 *pThis,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D        **ppTexture2D
	) {
	return oCreateTexture2D(pThis, pDesc, pInitialData, ppTexture2D);
}

static void VSSetConstantBuffers(
	ID3D11DeviceContext *pThis,
	UINT                StartSlot,
	UINT                NumBuffers,
	ID3D11Buffer *const *ppConstantBuffers
	) {

	//if ((StartSlot <= 1) && (StartSlot + NumBuffers >= 1)) {
	//	ID3D11Buffer* pBuffer = ppConstantBuffers[1 - StartSlot];
	//	D3D11_BUFFER_DESC bufferDesc;
	//	pBuffer->GetDesc(&bufferDesc);
	//	bufferDesc.BindFlags = 0;
	//	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
	//	bufferDesc.MiscFlags = 0;
	//	bufferDesc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;
	//	ComPtr<ID3D11Buffer> pBufferCopy;
	//	ComPtr<ID3D11Device> pDevice;
	//	pThis->GetDevice(pDevice.GetAddressOf());
	//	pDevice->CreateBuffer(&bufferDesc, NULL, pBufferCopy.GetAddressOf());
	//	pThis->CopyResource(pBufferCopy.Get(), pBuffer);

	//	D3D11_MAPPED_SUBRESOURCE res;
	//	if (SUCCEEDED(pThis->Map(pBufferCopy.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &res))) {
	//		/*float t = 1.0;
	//		for (int j = 0; j <= 16; j++) {
	//		LOG(LL_DBG, "j: ", j);
	//		DirectX::XMMATRIX matrix = DirectX::XMMATRIX((float*)res.pData + j);
	//		DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, matrix);
	//		DirectX::XMVECTOR vectors[6] = {
	//		{ t, 0.0f, 0.0f, 1.0f },
	//		{ -t, 0.0f, 0.0f, 1.0f },
	//		{ 0.0f, t, 0.0f, 1.0f },
	//		{ 0.0f, -t, 0.0f, 1.0f },
	//		{ 0.0f, 0.0f, t, 1.0f },
	//		{ 0.0f, 0.0f, -t, 1.0f }
	//		};

	//		for (int i = 0; i < 6; i++) {
	//		DirectX::XMVECTOR product = DirectX::XMVector4Transform(vectors[i], DirectX::XMMatrixTranspose(inverse));
	//		product /= DirectX::XMVectorGetW(product);
	//		LOG(LL_DBG, "i:", i, " ", DirectX::XMVectorGetByIndex(DirectX::XMVector4Length(product), 0));
	//		}
	//		LOG(LL_DBG, "#############################");
	//		}*/

	//		//LOG(LL_TRC, hexdump(res.pData, bufferDesc.ByteWidth));

	//		float* items = (float*)res.pData;
	//		for (int i = 0; i < bufferDesc.ByteWidth / sizeof(float); i++) {
	//			LOG(LL_DBG, i, ": ", items[i]);
	//		}
	//		pThis->Unmap(pBufferCopy.Get(), 0);
	//	}
	//}

	oVSSetConstantBuffers(pThis, StartSlot, NumBuffers, ppConstantBuffers);
}

static void Draw(
	ID3D11DeviceContext *pThis,
	UINT VertexCount,
	UINT StartVertexLocation
	) {
	oDraw(pThis, VertexCount, StartVertexLocation);
	if (pCtxLinearizeBuffer == pThis) {
		pCtxLinearizeBuffer = nullptr;
		LOG(LL_DBG, "Draw Called!!!");

		ComPtr<ID3D11Device> pDevice;
		pThis->GetDevice(pDevice.GetAddressOf());

		//ComPtr<ID3D11ShaderResourceView> pNewRSV;

		//ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

		//pThis->PSGetConstantBuffers(0)

		/*pThis->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, SRVs);
		int index = -1;
		for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
			if (SRVs[i]) {
				LOG(LL_TRC, "Shader resource \"", i, "\" exists.");
				ComPtr<ID3D11Resource> pResource;
				SRVs[i]->GetResource(pResource.GetAddressOf());
				if (pResource && (pResource.Get() == pGameDepthBufferResolved.Get())) {
					LOG(LL_TRC, "Shader resource for logarithmic texture found: ", i);
					index = i;
					D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
					SRVs[i]->GetDesc(&srvDesc);

					LOG_CALL(LL_DBG, pDevice->CreateShaderResourceView(pGameDepthBufferResolved.Get(), &srvDesc, pNewRSV.GetAddressOf()));
				}
				SRVs[i]->Release();
			}
		}

		if (index != -1) {
			LOG_CALL(LL_DBG, pThis->PSSetShaderResources(index, 1, pNewRSV.GetAddressOf()));
		}*/
		

		ComPtr<ID3D11RenderTargetView> pCurrentRTV;
		LOG_CALL(LL_DBG, pThis->OMGetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		pCurrentRTV->GetDesc(&rtvDesc);
		LOG_CALL(LL_DBG, pDevice->CreateRenderTargetView(pLinearDepthTexture.Get(), &rtvDesc, pCurrentRTV.ReleaseAndGetAddressOf()));
		LOG_CALL(LL_DBG, pThis->OMSetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL));

		D3D11_TEXTURE2D_DESC ldtDesc;
		pLinearDepthTexture->GetDesc(&ldtDesc);

		D3D11_VIEWPORT viewport;
		viewport.Width = ldtDesc.Width;
		viewport.Height = ldtDesc.Height;
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

static void DispatchIndirect(
	ID3D11DeviceContext* pThis,
	ID3D11Buffer *pBufferForArgs,
	UINT         AlignedByteOffsetForArgs
	) {
	/*D3D11_BUFFER_DESC desc;
	pBufferForArgs->GetDesc(&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Buffer> pBufferCopy;
	ComPtr<ID3D11Device> pDevice;
	pThis->GetDevice(pDevice.GetAddressOf());
	pDevice->CreateBuffer(&desc, NULL, pBufferCopy.GetAddressOf());
	pThis->CopyResource(pBufferCopy.Get(), pBufferForArgs);
	D3D11_MAPPED_SUBRESOURCE map;
	if (SUCCEEDED(pThis->Map(pBufferCopy.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &map))) {
		LOG(LL_DBG, hexdump(map.pData, desc.ByteWidth));
		pThis->Unmap(pBufferCopy.Get(), 0);
	}*/

	oDispatchIndirect(pThis, pBufferForArgs, AlignedByteOffsetForArgs);
	//LOG(LL_DBG, "DispatchIndirect ", AlignedByteOffsetForArgs);
}

static void Dispatch(
	ID3D11DeviceContext* pThis,
	UINT ThreadGroupCountX,
	UINT ThreadGroupCountY,
	UINT ThreadGroupCountZ
	) {
	oDispatch(pThis, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	/*ID3D11ShaderResourceView* pOldRTV[32];
	pThis->CSGetShaderResources(0, 32, pOldRTV);
	for (uint32_t i = 0; i < 32; i++) {
		if (pOldRTV[i]) {
			ComPtr<ID3D11Resource> pOldRTVTexture;
			pOldRTV[i]->GetResource(pOldRTVTexture.GetAddressOf());
			if (pOldRTVTexture == pGameDepthBufferQuarterLinear) {
				LOG(LL_DBG, i, " FUCK!!!");
			}
			pOldRTV[i]->Release();
		}
	}*/
	/*LOG(LL_DBG, "Dispatch:",
		" X:", ThreadGroupCountX,
		" Y:", ThreadGroupCountY,
		" Z:", ThreadGroupCountZ);*/
}

static float Detour_GetRenderTimeBase(int64_t choice) {
	std::pair<int32_t, int32_t> fps = config::fps;
	float result = 1000.0f * (float)fps.second / ((float)fps.first * ((float)config::motion_blur_samples + 1));
	LOG(LL_NFO, "Time step: ", result);
	return result;
}

//static float Detour_GetFrameRate(int32_t choice) {
//	std::pair<int32_t, int32_t> fps = Config::instance().getFPS();
//	return (float)fps.first / (float)fps.second;
//}
//
//static void Detour_getFrameRateFraction(float input, uint32_t* num, uint32_t* den) {
//	ogetFrameRateFraction(input, num, den);
//	*num = 120;
//	*den = 1;
//	LOG(LL_DBG, input, " ", *num, "/", *den);
//}
//
//static float Detour_Unk01(float x0, float x1, float x2, float x3) {
//	float result = oUnk01(x0, x1, x2, x3);
//	//LOG(LL_DBG, "(", x0, ",", x1, ",", x2, ",", x3, ") -> ", result);
//	return result;
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
			LOG(LL_DBG, desc.BindFlags);
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
		if (std::string("DepthBuffer_Resolved").compare(name) == 0) {
			pGameDepthBufferResolved = pTexture;
		} else if (std::string("Depth Quarter").compare(name) == 0) {
			pGameDepthBufferQuarter = pTexture;
		} else if (std::string("Depth Quarter Linear").compare(name) == 0) {
			pGameDepthBufferQuarterLinear = pTexture;
			D3D11_TEXTURE2D_DESC desc, resolvedDesc;

			pGameDepthBufferResolved->GetDesc(&resolvedDesc);
			pGameDepthBufferQuarterLinear->GetDesc(&desc);
			desc.Width = resolvedDesc.Width;
			desc.Height = resolvedDesc.Height;
			desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.CPUAccessFlags = 0;
			ComPtr<ID3D11Device> pDevice;
			pGameDepthBufferQuarterLinear->GetDevice(pDevice.GetAddressOf());
			LOG_CALL(LL_DBG, pDevice->CreateTexture2D(&desc, NULL, pLinearDepthTexture.GetAddressOf()));
		} else if (std::string("BackBuffer").compare(name) == 0) {
			pGameBackBuffer = pTexture;
		} else if (std::string("BackBuffer_Resolved").compare(name) == 0) {
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
				//exportContext->pDevice = pThis;
				/*exportContext->pDevice->GetImmediateContext(exportContext->pDeviceContext.GetAddressOf());*/
				//REQUIRE(pThis->CreateDeferredContext(0, session->pD3DCtx.GetAddressOf()), "Failed to create deferred context");
			/*	exportContext->captureRenderTargetViewReference = true;
				exportContext->captureDepthStencilViewReference = true;*/
				/*D3D11_TEXTURE2D_DESC* desc = (D3D11_TEXTURE2D_DESC*)pDesc;
				desc->Width = swapChainDesc.BufferDesc.Width;
				desc->Height = swapChainDesc.BufferDesc.Height;
				return oCreateTexture2D(pThis, &desc, pInitialData, ppTexture2D);*/
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

static void* Detour_LinearizeTexture(void* rcx, void* rdx) {
	void* result = oLinearizeTexture(rcx, rdx);
	//LOG(LL_DBG, __func__, ": ",
	//	" rcx:", rcx,
	//	" rdx:", rdx,
	//	" result:", result);
	return result;
}

//static void* Detour_GetGlobalVariableIndex(char* name, uint32_t edx) {
//	void* result = oGetGlobalVariableIndex(name, edx);
//
//	//if (name && (std::string("World").compare(name) == 0)) {
//	//	StackDump(64, "World:");
//	//}
//
//	LOG(LL_DBG, "GetSomething: ",
//		" name: ", name ? name : "<NULL>",
//		" result: ", result);
//
//	return result;
//}
//
//static void* Detour_GetVariable(char* name, void* unknown) {
//	void* result = oGetVariable(name, unknown);
//	/*LOG(LL_DBG, "GetVariable: ",
//		" name: ", name ? (void*)name : "<NULL>",
//		" unk:", unknown ? (char*)unknown : "<NULL>",
//		" result: ", result);*/
//	return result;
//}
//
//
//static void* Detour_GetMatrices(void* ecx, bool edx) {
//	/*LOG(LL_DBG, "GetMatrices(pre): ",
//		" *ecx:", *(float**)ecx,
//		" *ecx:", *(float**)ecx + 1,
//		" *ecx:", *(float**)ecx + 2);*/
//	LOG(LL_DBG, "prev", hexdump(ecx, 16 * 4 * 3));
//	void* result = oGetMatrices(ecx, edx);
//	LOG(LL_DBG, "post", hexdump(ecx, 16 * 4 * 3));
//	/*LOG(LL_DBG, "GetMatrices: ",
//		" ecx: ", ecx,
//		" edx:", edx,
//		" *ecx:", *(float**)ecx,
//		" *ecx:", *(float**)ecx + 1,
//		" *ecx:", *(float**)ecx + 2);*/
//	return result;
//}
//
//static uint32_t Detour_GetVarHash(char* name, uint32_t edx) {
//	uint32_t result = oGetVarHash(name, edx);
//	
//	/*if (name != NULL && std::string("World").compare(name) == 0) {
//		StackDump(64, "GetVarHash: ");
//	LOG(LL_DBG, "GetVarHash: ",
//		" name: ", name ? name : "<NULL>",
//		" edx: ", edx,
//		" result: ", Logger::hex(result, 8));
//	}*/
//
//
//	return result;
//}

//static void* Detour_GetVarPtrByHash(void* gUnk0, uint32_t hash) {
//	void* result = oGetVarPtrByHash(gUnk0, hash);
//
//	LOG(LL_DBG, "GetVarPtrByHash: ",
//		" gUnk0:", *(void**)gUnk0,
//		" hash: ", hash ? "0x" + Logger::hex(hash, 16) : "<NULL>",
//		" result: ", result);
//
//	return result;
//}
//
//static void* Detour_GetVarPtrByHash2(char* name) {
//	void* result = oGetVarPtrByHash2(name);
//
//	LOG(LL_DBG, "GetVarPtrByHash2: ",
//		" name:", name ? name : "<NULL>",
//		" result: ", result);
//
//	return result;
//}

/*ID3D11ShaderResourceView* pViewArray[10] = { 0 };
pThis->VSGetShaderResources(0, 10, &pViewArray[0]);
for (int i = 0; i < 10; i++) {
if (pViewArray[i] == NULL) {
LOG(LL_DBG, "Null:", i);
continue;
}
D3D11_SHADER_RESOURCE_VIEW_DESC desc;
pViewArray[i]->GetDesc(&desc);

ComPtr<ID3D11Texture2D> pTexture;
ComPtr<ID3D11Texture3D> pTexture3D;

D3D11_TEXTURE2D_DESC tdesc;
D3D11_TEXTURE3D_DESC t3d;

switch (desc.ViewDimension) {
case D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
case D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D:
pViewArray[i]->GetResource(((ID3D11Resource**)pTexture.GetAddressOf()));
pTexture->GetDesc(&tdesc);
LOG(LL_DBG,
"Texture2D: ",
" as:", tdesc.ArraySize,
" fmt:", conv_dxgi_format_to_string(tdesc.Format),
" w:", tdesc.Width,
" h:", tdesc.Height
);
break;
case D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE3D:
pViewArray[i]->GetResource(((ID3D11Resource**)pTexture3D.GetAddressOf()));
pTexture3D->GetDesc(&t3d);
LOG(LL_DBG,
"Texture3D: ",
" fmt:", conv_dxgi_format_to_string(t3d.Format),
" w:", t3d.Width,
" h:", t3d.Height,
" d:", t3d.Depth
);
}



pViewArray[i]->Release();
}*/


//ID3D11Buffer* pBufferArray[15];
//pThis->VSGetConstantBuffers(0, 15, &pBufferArray[0]);
//for (int k = 0; k < 15; k++) {
//	if (pBufferArray[k] == NULL) {
//		continue;
//	}
//	LOG(LL_DBG, "Buffer #", k);
//	D3D11_BUFFER_DESC bufferDesc;
//	pBufferArray[k]->GetDesc(&bufferDesc);
//	bufferDesc.BindFlags = 0;
//	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
//	bufferDesc.MiscFlags = 0;
//	bufferDesc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;
//	ComPtr<ID3D11Buffer> pBufferCopy;
//	exportContext->pDevice->CreateBuffer(&bufferDesc, NULL, pBufferCopy.GetAddressOf());
//	pThis->CopyResource(pBufferCopy.Get(), pBufferArray[k]);

//	D3D11_MAPPED_SUBRESOURCE res;
//	if (SUCCEEDED(pThis->Map(pBufferCopy.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &res))) {
//		/*float t = 1.0;
//		for (int j = 0; j <= 16; j++) {
//			LOG(LL_DBG, "j: ", j);
//			DirectX::XMMATRIX matrix = DirectX::XMMATRIX((float*)res.pData + j);
//			DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, matrix);
//			DirectX::XMVECTOR vectors[6] = {
//				{ t, 0.0f, 0.0f, 1.0f },
//				{ -t, 0.0f, 0.0f, 1.0f },
//				{ 0.0f, t, 0.0f, 1.0f },
//				{ 0.0f, -t, 0.0f, 1.0f },
//				{ 0.0f, 0.0f, t, 1.0f },
//				{ 0.0f, 0.0f, -t, 1.0f }
//			};

//			for (int i = 0; i < 6; i++) {
//				DirectX::XMVECTOR product = DirectX::XMVector4Transform(vectors[i], DirectX::XMMatrixTranspose(inverse));
//				product /= DirectX::XMVectorGetW(product);
//				LOG(LL_DBG, "i:", i, " ", DirectX::XMVectorGetByIndex(DirectX::XMVector4Length(product), 0));
//			}
//			LOG(LL_DBG, "#############################");
//		}*/

//		LOG(LL_TRC, hexdump(res.pData, bufferDesc.ByteWidth));

//		float* items = (float*)res.pData;
//		for (int i = 0; i < bufferDesc.ByteWidth / sizeof(float); i++) {
//			LOG(LL_DBG, i, ": ", items[i]);
//		}
//		pThis->Unmap(pBufferCopy.Get(), 0);
//	}
//	pBufferArray[k]->Release();
//}

/*uint32_t id = Detour_GetVarHash("gClipPlanes", 0);
if (id) {
void* ptr = getGlobalPtr(id);
if (ptr) {
LOG(LL_DBG, ptr);
LOG(LL_DBG, hexdump(ptr, 16 * 4 * 3));
}
}*/