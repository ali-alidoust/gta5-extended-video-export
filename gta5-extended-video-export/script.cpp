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
	
	
	std::shared_ptr<PLH::IATHook> hkCoCreateInstance(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateSinkWriterFromURL(new PLH::IATHook);

	std::shared_ptr<PLH::X64Detour> hkGetFrameRateFraction(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetRenderTimeBase(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetFrameRate(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetAudioSamples(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkUnk01(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateTexture(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkCreateExportTexture(new PLH::X64Detour);

	/*std::shared_ptr<PLH::X64Detour> hkGetGlobalVariableIndex(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVariable(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetMatrices(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVar(new PLH::X64Detour);
	std::shared_ptr<PLH::X64Detour> hkGetVarPtrByHash(new PLH::X64Detour);*/
	ComPtr<ID3D11Texture2D> pGameDepthBuffer;
	ComPtr<ID3D11Texture2D> pGameBackBuffer;
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
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 33, &Hook_OMSetRenderTargets, &oOMSetRenderTargets, hkOMSetRenderTargets), "Failed to hook ID3DDeviceContext::OMSetRenderTargets");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 44, &RSSetViewports, &oRSSetViewports, hkRSSetViewports), "Failed to hook ID3DDeviceContext::RSSetViewports"); 
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 45, &RSSetScissorRects, &oRSSetScissorRects, hkRSSetScissorRects), "Failed to hook ID3DDeviceContext::RSSetViewports"); 

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
		pYaraHelper->addEntry("yara_get_render_time_base_function", yara_get_render_time_base_function, &pGetRenderTimeBase);
		//pYaraHelper->addEntry("yara_create_export_texture_function", yara_create_export_texture_function, &pCreateExportTexture);
		pYaraHelper->addEntry("yara_create_texture_function", yara_create_texture_function, &pCreateTexture);
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
	char buffer[64];
	GUIDToString(rclsid, buffer, 64);
	LOG(LL_NFO, "CoCreateInstance: ", buffer);
	POST();
	return result;
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
	ComPtr<ID3D11Resource> pRTVTexture;
	if ((ppRenderTargetViews) && (ppRenderTargetViews[0])) {
		ppRenderTargetViews[0]->GetResource(pRTVTexture.GetAddressOf());
	}
	if ((exportContext != NULL) && (exportContext->pExportRenderTarget != NULL) && (exportContext->pExportRenderTarget == pRTVTexture)) {
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
						pGameDepthBuffer->GetDesc(&desc);
						desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
						desc.BindFlags = 0;
						desc.MiscFlags = 0;
						desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

						REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pDepthBufferCopy.GetAddressOf()), "Failed to create depth buffer copy texture");

						pThis->CopyResource(pDepthBufferCopy.Get(), pGameDepthBuffer.Get());
					}
					{
						D3D11_TEXTURE2D_DESC desc;
						pGameBackBuffer->GetDesc(&desc);
						desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
						desc.BindFlags = 0;
						desc.MiscFlags = 0;
						desc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;

						REQUIRE(pDevice->CreateTexture2D(&desc, NULL, pBackBufferCopy.GetAddressOf()), "Failed to create back buffer copy texture");

						pThis->CopyResource(pBackBufferCopy.Get(), pGameBackBuffer.Get());
					}
					session->enqueueEXRImage(pThis, pBackBufferCopy, pDepthBufferCopy);
				}
				LOG_CALL(LL_DBG,exportContext->pSwapChain->Present(0, DXGI_PRESENT_TEST)); // IMPORTANT: This call makes ENB and ReShade effects to be applied to the render target

				ComPtr<ID3D11Texture2D> pSwapChainBuffer;
				REQUIRE(exportContext->pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pSwapChainBuffer.GetAddressOf()), "Failed to get swap chain's buffer");
								
				auto& image_ref = *(exportContext->capturedImage);
				LOG_CALL(LL_DBG, DirectX::CaptureTexture(exportContext->pDevice.Get(), exportContext->pDeviceContext.Get(), pSwapChainBuffer.Get(), image_ref));
				if (exportContext->capturedImage->GetImageCount() == 0) {
					LOG(LL_ERR, "There is no image to capture.");
					throw std::exception();
				}
				const DirectX::Image* image = exportContext->capturedImage->GetImage(0, 0, 0);
				NOT_NULL(image, "Could not get current frame.");
				NOT_NULL(image->pixels, "Could not get current frame.");

				REQUIRE(session->enqueueVideoFrame(image->pixels, (int)(image->width * image->height * 4)), "Failed to enqueue frame");
				exportContext->capturedImage->Release();
			} catch (std::exception&) {
				LOG(LL_ERR, "Reading video frame from D3D Device failed.");
				exportContext->capturedImage->Release();
				LOG_CALL(LL_DBG, session.reset());
				LOG_CALL(LL_DBG, exportContext.reset());
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
			exportContext->videoMediaType = pInputMediaType;
		} else if (IsEqualGUID(majorType, MFMediaType_Audio)) {
			try {
				std::lock_guard<std::mutex> sessionLock(mxSession);

				// Create Video Context
				{
					UINT width, height, fps_num, fps_den;
					MFGetAttribute2UINT32asUINT64(exportContext->videoMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
					MFGetAttributeRatio(exportContext->videoMediaType.Get(), MF_MT_FRAME_RATE, &fps_num, &fps_den);

					GUID pixelFormat;
					exportContext->videoMediaType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

					DXGI_SWAP_CHAIN_DESC desc;
					exportContext->pSwapChain->GetDesc(&desc);


					
					if (isCustomFrameRateSupported) {
						auto fps = config::fps;
						fps_num = fps.first;
						fps_den = fps.second;


						if (((float)fps.first * ((float)config::motion_blur_samples + 1) / (float)fps.second) > 60.0f) {
							LOG(LL_NON, "fps * (motion_blur_samples + 1) > 60.0!!!");
							LOG(LL_NON, "Audio export will be disabled!!!");
							exportContext->isAudioExportDisabled = true;
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
				LOG_CALL(LL_DBG, exportContext.reset());
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
	if ((session != NULL) && (dwStreamIndex == 1) && (!exportContext->isAudioExportDisabled)) {

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
			LOG_CALL(LL_DBG, exportContext.reset());
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
	LOG_CALL(LL_DBG, exportContext.reset());
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

	StackDump(64, "VSSetConstantBuffers");

	if ((StartSlot <= 1) && (StartSlot + NumBuffers >= 1)) {
		ID3D11Buffer* pBuffer = ppConstantBuffers[1 - StartSlot];
		D3D11_BUFFER_DESC bufferDesc;
		pBuffer->GetDesc(&bufferDesc);
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.Usage = D3D11_USAGE::D3D11_USAGE_STAGING;
		ComPtr<ID3D11Buffer> pBufferCopy;
		ComPtr<ID3D11Device> pDevice;
		pThis->GetDevice(pDevice.GetAddressOf());
		pDevice->CreateBuffer(&bufferDesc, NULL, pBufferCopy.GetAddressOf());
		pThis->CopyResource(pBufferCopy.Get(), pBuffer);

		D3D11_MAPPED_SUBRESOURCE res;
		if (SUCCEEDED(pThis->Map(pBufferCopy.Get(), 0, D3D11_MAP::D3D11_MAP_READ, 0, &res))) {
			/*float t = 1.0;
			for (int j = 0; j <= 16; j++) {
			LOG(LL_DBG, "j: ", j);
			DirectX::XMMATRIX matrix = DirectX::XMMATRIX((float*)res.pData + j);
			DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, matrix);
			DirectX::XMVECTOR vectors[6] = {
			{ t, 0.0f, 0.0f, 1.0f },
			{ -t, 0.0f, 0.0f, 1.0f },
			{ 0.0f, t, 0.0f, 1.0f },
			{ 0.0f, -t, 0.0f, 1.0f },
			{ 0.0f, 0.0f, t, 1.0f },
			{ 0.0f, 0.0f, -t, 1.0f }
			};

			for (int i = 0; i < 6; i++) {
			DirectX::XMVECTOR product = DirectX::XMVector4Transform(vectors[i], DirectX::XMMatrixTranspose(inverse));
			product /= DirectX::XMVectorGetW(product);
			LOG(LL_DBG, "i:", i, " ", DirectX::XMVectorGetByIndex(DirectX::XMVector4Length(product), 0));
			}
			LOG(LL_DBG, "#############################");
			}*/

			//LOG(LL_TRC, hexdump(res.pData, bufferDesc.ByteWidth));

			float* items = (float*)res.pData;
			for (int i = 0; i < bufferDesc.ByteWidth / sizeof(float); i++) {
				LOG(LL_DBG, i, ": ", items[i]);
			}
			pThis->Unmap(pBufferCopy.Get(), 0);
		}
	}

	oVSSetConstantBuffers(pThis, StartSlot, NumBuffers, ppConstantBuffers);
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
	PRE();
	void* result = oCreateTexture(rcx, name, r8d, width, height, format, rsp30);

	void** vresult = (void**)result;

	LOG(LL_TRC, "CreateExportTexture:",
		" rcx:", rcx,
		" name:", name ? name : "<NULL>",
		" r8d:", Logger::hex(r8d, 8),
		" width:", width,
		" height:", height,
		" fmt:", format,
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

	if ((name != NULL) && (result != NULL)) {
		IUnknown* pUnknown = (IUnknown*)(*(vresult + 7));
		if (std::string("DepthBuffer_Resolved").compare(name) == 0) {
			pUnknown->QueryInterface(pGameDepthBuffer.GetAddressOf());
		} else if (std::string("BackBuffer_Resolved").compare(name) == 0) {
			pUnknown->QueryInterface(pGameBackBuffer.GetAddressOf());
			D3D11_TEXTURE2D_DESC desc;
			pGameBackBuffer->GetDesc(&desc);
			LOG(LL_DBG, "BackBuffer: fmt", conv_dxgi_format_to_string(desc.Format));
		} else if (std::string("VideoEncode").compare(name) == 0) {
			ComPtr<ID3D11Texture2D> pExportTexture;
			pUnknown->QueryInterface(pExportTexture.GetAddressOf());
			DXGI_SWAP_CHAIN_DESC swapChainDesc;
			REQUIRE(mainSwapChain->GetDesc(&swapChainDesc), "Failed to get swap chain descriptor");

			D3D11_TEXTURE2D_DESC desc;
			pExportTexture->GetDesc(&desc);

			LOG(LL_NFO, "Detour_CreateTexture: fmt:", conv_dxgi_format_to_string(desc.Format),
				" w:", desc.Width,
				" h:", desc.Height);
			std::lock_guard<std::mutex> sessionLock(mxSession);
			LOG_CALL(LL_DBG, exportContext.reset());
			LOG_CALL(LL_DBG, session.reset());
			try {
				LOG(LL_NFO, "Creating session...");

				if (config::auto_reload_config) {
					LOG_CALL(LL_DBG, config::reload());
				}

				session.reset(new Encoder::Session());
				NOT_NULL(session, "Could not create the session");
				exportContext.reset(new ExportContext());
				NOT_NULL(exportContext, "Could not create export context");
				exportContext->pSwapChain = mainSwapChain;
				exportContext->pExportRenderTarget = pExportTexture;

				
				pExportTexture->GetDevice(exportContext->pDevice.GetAddressOf());
				exportContext->pDevice->GetImmediateContext(exportContext->pDeviceContext.GetAddressOf());
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
				LOG_CALL(LL_DBG, exportContext.reset());
			}
			//}
		}
	}

	//if (*(vrcx + 2)) {
	//	LOG(LL_DBG, 1);
	//	ComPtr<ID3D11Texture2D> pDeferredContext;
	//	IUnknown* pUnknown = (IUnknown*)(*(void**)*((void**)(vrcx + 2)+1));
	//	LOG(LL_DBG, 2);
	//	if (SUCCEEDED(pUnknown->QueryInterface(pDeferredContext.GetAddressOf()))) {
	//		LOG(LL_DBG, "Yeah!");

	//		D3D11_TEXTURE2D_DESC desc;
	//		pDeferredContext->GetDesc(&desc);
	//		LOG(LL_DBG, conv_dxgi_format_to_string(desc.Format));
	//		//ComPtr<ID3D11RenderTargetView> pRTV;
	//		/*pDeferredContext->OMGetRenderTargets(1, pRTV.GetAddressOf(), NULL);
	//		if (pRTV) {
	//			D3D11_RENDER_TARGET_VIEW_DESC desc;
	//			pRTV->GetDesc(&desc);
	//			LOG(LL_DBG, conv_dxgi_format_to_string(desc.Format));
	//		}*/
	//	}
	//	LOG(LL_DBG, 3);
	//	//D3D11_TEXTURE2D_DESC desc;
	//	LOG(LL_DBG, 4);
	//	//LOG(LL_DBG, "BackBuffer: fmt", conv_dxgi_format_to_string(desc.Format));
	//}

	POST();
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