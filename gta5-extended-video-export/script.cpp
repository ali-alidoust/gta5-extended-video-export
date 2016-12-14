// script.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "custom-hooks.h"
#include "script.h"
#include "MFUtility.h"
#include "encoder.h"
#include "logger.h"
#include "config.h"
#include "util.h"
#include "yara-patterns.h"

#include "..\DirectXTex\DirectXTex\DirectXTex.h"
#include "hook-def.h"

using namespace Microsoft::WRL;

namespace {
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessInput(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFTransform_ProcessMessage(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_AddStream(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_SetInputMediaType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_WriteSample(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_Finalize(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkClearRenderTargetView(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkOMSetRenderTargets(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateRenderTargetView(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateDepthStencilView(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkRSSetViewports(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateObjectFromURL(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkRSSetScissorRects(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkLock(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkGetData(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCreateTexture2D(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkMap(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCopyResource(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkCopySubresourceRegion(new PLH::VFuncDetour);

	std::shared_ptr<PLH::IATHook> hkCoCreateInstance(new PLH::IATHook);
	std::shared_ptr<PLH::IATHook> hkMFCreateSinkWriterFromURL(new PLH::IATHook);

	std::unique_ptr<Encoder::Session> session;
	std::mutex mxSession;
	//std::mutex mxLatestImage;
	//ComPtr<ID3D11Texture2D> pPrimaryRenderTarget;
	//ComPtr<ID3D11Texture2D> pBackbufferRenderTarget = NULL;
	//std::map<std::thread::id, ID3D11RenderTargetView*> createdRTVs;
	//std::map<std::thread::id, ID3D11DepthStencilView*> createdDSVs;
	std::thread::id mainThreadId;
	ComPtr<IDXGISwapChain> mainSwapChain;

	struct ExportContext {

		ExportContext() {
			PRE();
			POST();
		}

		~ExportContext() {
			PRE();
			POST();
		}

		bool captureRenderTargetViewReference = false;
		bool captureDepthStencilViewReference = false;
		ComPtr<ID3D11RenderTargetView> pExportRenderTargetView;
		ComPtr<ID3D11Texture2D> pExportRenderTarget;
		ComPtr<ID3D11Texture2D> pExportDepthStencil;
		ComPtr<ID3D11DeviceContext> pDeviceContext;
		ComPtr<ID3D11Device> pDevice;
		std::shared_ptr<DirectX::ScratchImage> latestImage = std::make_shared<DirectX::ScratchImage>();
		//DirectX::ScratchImage latestImage;

		UINT width;
		UINT height;
		UINT outputWidth;
		UINT outputHeight;

		UINT pts = 0;

		ComPtr<IMFMediaType> videoMediaType;
	};

	std::shared_ptr<ExportContext> exportContext;
	YR_COMPILER* pYrCompiler;
}

tCoCreateInstance oCoCreateInstance;
tMFCreateSinkWriterFromURL oMFCreateSinkWriterFromURL;
tIMFTransform_ProcessInput oIMFTransform_ProcessInput;
tIMFTransform_ProcessMessage oIMFTransform_ProcessMessage;
tIMFSinkWriter_SetInputMediaType oIMFSinkWriter_SetInputMediaType;
tIMFSinkWriter_AddStream oIMFSinkWriter_AddStream;
tIMFSinkWriter_WriteSample oIMFSinkWriter_WriteSample;
tIMFSinkWriter_Finalize oIMFSinkWriter_Finalize;
tClearRenderTargetView oClearRenderTargetView;
tOMSetRenderTargets oOMSetRenderTargets;
tCreateTexture2D oCreateTexture2D;
tCreateRenderTargetView oCreateRenderTargetView;
tCreateDepthStencilView oCreateDepthStencilView;
tCreateObjectFromURL oCreateObjectFromURL;
tRSSetViewports oRSSetViewports;
tRSSetScissorRects oRSSetScissorRects;
tLock oLock;
tMap oMap;
tGetData oGetData;
tCopyResource oCopyResource;
tCopySubresourceRegion oCopySubresourceRegion;

void avlog_callback(void *ptr, int level, const char* fmt, va_list vargs) {
	//PRE();
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
	//POST();
}

void onPresent(IDXGISwapChain *swapChain) {
	mainSwapChain = swapChain;
	static bool once = true;
	if (once) {
		try {
			ComPtr<ID3D11Device> pDevice;
			ComPtr<ID3D11DeviceContext> pDeviceContext;
			ComPtr<ID3D11Texture2D> texture;
			DXGI_SWAP_CHAIN_DESC desc;

			swapChain->GetDesc(&desc);
			LOG(LL_NFO, "BUFFER COUNT: ", desc.BufferCount);
			REQUIRE(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)texture.GetAddressOf()), "Failed to get the texture buffer");
			//pPrimaryRenderTarget = texture;
			REQUIRE(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)pDevice.GetAddressOf()), "Failed to get the D3D11 device");
			pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
			NOT_NULL(pDeviceContext.Get(), "Failed to get D3D11 device context");
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 9, &Hook_PSSetShader, &oPSSetShader, hkPSSetShader), "Failed to hook ID3DDeviceContext::PSSetShader", std::exception());
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 13, &Hook_Draw, &oDraw, hkDraw), "Failed to hook ID3DDeviceContext::Draw", std::exception());
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 33, &Hook_OMSetRenderTargets, &oOMSetRenderTargets, hkOMSetRenderTargets), "Failed to hook ID3DDeviceContext::OMSetRenderTargets");
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 44, &RSSetViewports, &oRSSetViewports, hkRSSetViewports), "Failed to hook ID3DDeviceContext::RSSetViewports", std::exception());
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 45, &RSSetScissorRects, &oRSSetScissorRects, hkRSSetScissorRects), "Failed to hook ID3DDeviceContext::RSSetViewports", std::exception());
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 29, &GetData, &oGetData, hkGetData), "Failed to hook ID3DDeviceContext::GetData", std::exception());
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 14, &Hook_Map, &oMap, hkMap), "Failed to hook ID3DDeviceContext::Map", std::exception());
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 47, &CopyResource, &oCopyResource, hkCopyResource), "Failed to hook ID3DDeviceContext::Map", std::exception());
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 46, &CopySubresourceRegion, &oCopySubresourceRegion, hkCopySubresourceRegion), "Failed to hook ID3DDeviceContext::Map", std::exception());
			REQUIRE(hookVirtualFunction(pDevice.Get(), 5, &Hook_CreateTexture2D, &oCreateTexture2D, hkCreateTexture2D), "Failed to hook ID3DDeviceContext::Map");
			REQUIRE(hookVirtualFunction(pDevice.Get(), 9, &Hook_CreateRenderTargetView, &oCreateRenderTargetView, hkCreateRenderTargetView), "Failed to hook ID3DDeviceContext::CreateRenderTargetView");
			REQUIRE(hookVirtualFunction(pDevice.Get(), 10, &Hook_CreateDepthStencilView, &oCreateDepthStencilView, hkCreateDepthStencilView), "Failed to hook ID3DDeviceContext::CreateDepthStencilView");
			//REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 50, &Hook_ClearRenderTargetView, &oClearRenderTargetView, hkClearRenderTargetView), "Failed to hook ID3DDeviceContext::Draw", std::exception());
			once = false;
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}

	//if ((session != NULL) && session->isCapturing) {
	//	try {
	//		std::lock_guard<std::mutex> lock_guard(mxLatestImage);
	//		ComPtr<ID3D11Device> pDevice;
	//		ComPtr<ID3D11DeviceContext> pDeviceContext;
	//		ComPtr<ID3D11Texture2D> texture;
	//		REQUIRE(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)texture.GetAddressOf()), "Failed to get the texture buffer", std::exception());
	//		REQUIRE(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)pDevice.GetAddressOf()), "Failed to get the D3D11 device", std::exception());
	//		pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
	//		NOT_NULL(pDeviceContext.Get(), "Failed to get D3D11 device context", std::exception());

	//		/*ID3D11ShaderResourceView* pShaderViews_unsafe;
	//		pDeviceContext->PSGetShaderResources(0, 1, &pShaderViews_unsafe);
	//		ComPtr<ID3D11Resource> resource;
	//		ComPtr<ID3D11Texture2D> texture2d;
	//		pShaderViews_unsafe[0].GetResource(resource.GetAddressOf());
	//		if (SUCCEEDED(resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf()))) {
	//			DirectX::CaptureTexture(pDevice.Get(), pDeviceContext.Get(), texture2d.Get(), latestImage);
	//		}*/


	//		ID3D11RenderTargetView* pViews_unsafe;
	//		ID3D11DepthStencilView* pDepth_unsafe;
	//		UINT numViews = 1;
	//		pDeviceContext->OMGetRenderTargets(numViews, &pViews_unsafe, &pDepth_unsafe);
	//		int i = 0;
	//		//for (int i = 0; i < numViews; i++) {
	//			ComPtr<ID3D11Resource> resource;
	//			ComPtr<ID3D11Texture2D> texture2d;
	//			pViews_unsafe[0].GetResource(resource.GetAddressOf());
	//			if (SUCCEEDED(resource.As(&texture2d))) {
	//				//DirectX::CaptureTexture(pDevice.Get(), pDeviceContext.Get(), texture2d.Get(), latestImage);
	//				DirectX::CaptureTexture(pDevice.Get(), pDeviceContext.Get(), pExportRenderTarget.Get(), latestImage);
	//			}
	//		//}

	//		//REQUIRE(DirectX::CaptureTexture(pDevice.Get(), pDeviceContext.Get(), texture.Get(), latestImage), "Failed to capture the texture", std::exception());
	//	} catch (std::exception& ex) {
	//		latestImage.Release();
	//	}
	//}
	//GRAPHICS::DRAW_RECT(0.5f, 0.5f, 0.01f, 0.01f, 0, 255, 0, 255);
}

void initialize() {
	PRE();
	//hookNamedFunction("kernel32.dll", "LoadLibraryA", &Hook_LoadLibraryA, &oLoadLibraryA, hkLoadLibraryA);
	//hookNamedFunction("kernel32.dll", "EnterCriticalSection", &Hook_RtlEnterCriticalSection, &oRtlEnterCriticalSection, hkHook_RtlEnterCriticalSection);
	try {
		mainThreadId = std::this_thread::get_id();
		ComPtr<IMFSourceResolver> pSourceResolver;
		MFCreateSourceResolver(pSourceResolver.GetAddressOf());
		REQUIRE(hookVirtualFunction(pSourceResolver.Get(), 3, &CreateObjectFromURL, &oCreateObjectFromURL, hkCreateObjectFromURL), "Failed to hook IMFSourceResolver::CreateObjectFromURL");

		ComPtr<IMFMediaBuffer> pMediaBuffer;
		MFCreateMemoryBuffer(1, pMediaBuffer.GetAddressOf());
		REQUIRE(hookVirtualFunction(pMediaBuffer.Get(), 3, &Lock, &oLock, hkLock), "Failed to hook IMFSourceResolver::CreateObjectFromURL");

		REQUIRE(hookNamedFunction("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &Hook_MFCreateSinkWriterFromURL, &oMFCreateSinkWriterFromURL, hkMFCreateSinkWriterFromURL), "Failed to hook MFCreateSinkWriterFromURL in mfreadwrite.dll");
		REQUIRE(hookNamedFunction("ole32.dll", "CoCreateInstance", &Hook_CoCreateInstance, &oCoCreateInstance, hkCoCreateInstance), "Failed to hook CoCreateInstance in ole32.dll");

			
		LOG_CALL(LL_DBG, yr_initialize());
		REQUIRE(yr_compiler_create(&pYrCompiler), "Failed to create yara compiler.");
		REQUIRE(yr_compiler_add_string(pYrCompiler, yara_resolution_fields.c_str(), NULL), "Failed to compile yara rule");


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
	//LOG("Starting script...");
	LOG(LL_NFO, "Starting main loop");
	while (true) {
		WAIT(0);
	}
	POST();
}

static HRESULT CreateObjectFromURL(
	IMFSourceResolver *pThis,
	LPCWSTR           pwszURL,
	DWORD             dwFlags,
	IPropertyStore    *pProps,
	MF_OBJECT_TYPE    *pObjectType,
	IUnknown          **ppObject
	) {
	
	return oCreateObjectFromURL(pThis, pwszURL, dwFlags, pProps, pObjectType, ppObject);
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

	if (IsEqualCLSID(rclsid, CLSID_CMSH264EncoderMFT)) {
		LOG_IF_FAILED(hookVirtualFunction((*ppv), 23, &Hook_IMFTransform_ProcessMessage, &oIMFTransform_ProcessMessage, hkIMFTransform_ProcessMessage), "Failed to hook IMFTransform::ProcessMessage");
		LOG_IF_FAILED(hookVirtualFunction((*ppv), 24, &Hook_IMFTransform_ProcessInput, &oIMFTransform_ProcessInput, hkIMFTransform_ProcessInput), "Failed to hook IMFTransform::ProcessInput");
	}

	POST();
	return result;
}

static void CopyResource(
	ID3D11DeviceContext *pThis,
	ID3D11Resource      *pDstResource,
	ID3D11Resource      *pSrcResource
	) {
	return oCopyResource(pThis, pDstResource, pSrcResource);
}

static void CopySubresourceRegion(
	ID3D11DeviceContext  *pThis,
	ID3D11Resource       *pDstResource,
	UINT                 DstSubresource,
	UINT                 DstX,
	UINT                 DstY,
	UINT                 DstZ,
	ID3D11Resource       *pSrcResource,
	UINT                 SrcSubresource,
	const D3D11_BOX      *pSrcBox
	) {
	/*if ((exportContext != NULL) && (exportContext->pExportRenderTarget != NULL)) {
		if ((void*)pSrcResource == (void*)exportContext->pExportRenderTarget.Get()) {
			LOG("Copying from texture2D");
		}
		if ((void*)pDstResource == (void*)exportContext->pExportRenderTarget.Get()) {
			LOG("Copying to texture2D");
		}
	}*/
	return oCopySubresourceRegion(pThis, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}

static HRESULT Hook_CreateRenderTargetView(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView        **ppRTView
	) {
	PRE();
	if ((exportContext != NULL) && exportContext->captureRenderTargetViewReference && (std::this_thread::get_id() == mainThreadId)) {
		LOG(LL_NFO, "Capturing export render target view...");
		exportContext->captureRenderTargetViewReference = false;
		exportContext->pDevice = pThis;
		exportContext->pDevice->GetImmediateContext(exportContext->pDeviceContext.GetAddressOf());

		ComPtr<ID3D11Texture2D> pOldTexture;
		if (SUCCEEDED(pResource->QueryInterface(pOldTexture.GetAddressOf()))) {
			D3D11_TEXTURE2D_DESC desc;
			pOldTexture->GetDesc(&desc);
			exportContext->outputWidth = desc.Width;
			exportContext->outputHeight = desc.Height;
			/*if ((exportContext->width != 0) && (exportContext->height != 0)) {
				desc.Width = exportContext->width;
				desc.Height = exportContext->height;
			}*/
			ComPtr<ID3D11Texture2D> pTexture;
			pThis->CreateTexture2D(&desc, NULL, pTexture.GetAddressOf());
			ComPtr<ID3D11Resource> pRes;
			if (SUCCEEDED(pTexture.As(&pRes))) {
				exportContext->pExportRenderTarget = pTexture;
				HRESULT result = oCreateRenderTargetView(pThis, pRes.Get(), pDesc, ppRTView);
				exportContext->pExportRenderTargetView = *(ppRTView);
				POST();
				return result;
			}
		}
	}
	//LOG("createRTV: ", std::this_thread::get_id());
	POST();
	return oCreateRenderTargetView(pThis, pResource, pDesc, ppRTView);
	//createdRTVs[std::this_thread::get_id()] = *ppRTView;
}

static HRESULT Hook_CreateDepthStencilView(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	ID3D11DepthStencilView        **ppDepthStencilView
	) {
	if ((exportContext != NULL) && exportContext->captureDepthStencilViewReference && mainThreadId == std::this_thread::get_id()) {
		try {
			exportContext->captureDepthStencilViewReference = false;
			LOG(LL_NFO, "Capturing export depth stencil view...");
			ComPtr<ID3D11Texture2D> pOldTexture;
			REQUIRE(pResource->QueryInterface(pOldTexture.GetAddressOf()), "Failed to get depth stencil texture");
			D3D11_TEXTURE2D_DESC desc;
			pOldTexture->GetDesc(&desc);
			/*if ((exportContext->width != 0) && (exportContext->height != 0)) {
				desc.Width = exportContext->width;
				desc.Height = exportContext->height;
			}*/
			ComPtr<ID3D11Texture2D> pTexture;
			pThis->CreateTexture2D(&desc, NULL, pTexture.GetAddressOf());

			ComPtr<ID3D11Resource> pRes;
			REQUIRE(pTexture.As(&pRes), "Failed to convert ID3D11Texture to ID3D11Resource.");
			exportContext->pExportDepthStencil = pTexture;
			return oCreateDepthStencilView(pThis, pRes.Get(), pDesc, ppDepthStencilView);
		} catch (...) {
			LOG(LL_ERR, "Failed to capture depth stencil view");
		}
	}
	//LOG("createDSV: ", std::this_thread::get_id());
	HRESULT result = oCreateDepthStencilView(pThis, pResource, pDesc, ppDepthStencilView);
	//createdDSVs[std::this_thread::get_id()] = *ppDepthStencilView;
	return result;
}

static void RSSetViewports(
	ID3D11DeviceContext  *pThis,
	UINT                 NumViewports,
	const D3D11_VIEWPORT *pViewports
	) {
	if ((exportContext != NULL) && isCurrentRenderTargetView(pThis, exportContext->pExportRenderTargetView)) {
		D3D11_VIEWPORT* pNewViewports = new D3D11_VIEWPORT[NumViewports];
		for (UINT i = 0; i < NumViewports; i++) {
			pNewViewports[i] = pViewports[i];
			if ((exportContext->width != 0) && (exportContext->height != 0)) {
				/*pNewViewports[i].Width = (float)exportContext->width;
				pNewViewports[i].Height = (float)exportContext->height;*/
			}
		}
		return oRSSetViewports(pThis, NumViewports, pNewViewports);
	}

	/*if ((session != NULL) && (session->isCapturing)) {
		for (int i = 0; i < NumViewports; i++) {
			LOG("[", i + 1, "/", NumViewports,
				"] -> x:", pViewports[i].TopLeftX,
				" y:", pViewports[i].TopLeftY,
				" w:", pViewports[i].Width,
				" h:", pViewports[i].Height,
				" minz:", pViewports[i].MinDepth,
				" maxz:", pViewports[i].MaxDepth);
		}
	}*/
	//LOG(__func__);
	return oRSSetViewports(pThis, NumViewports, pViewports);
}

static void RSSetScissorRects(
	ID3D11DeviceContext *pThis,
	UINT          NumRects,
	const D3D11_RECT *  pRects
	) {
	if ((exportContext != NULL) && isCurrentRenderTargetView(pThis, exportContext->pExportRenderTargetView)) {
		D3D11_RECT* pNewRects = new D3D11_RECT[NumRects];
		for (UINT i = 0; i < NumRects; i++) {
			pNewRects[i] = pRects[i];
			if ((exportContext->width != 0) && (exportContext->height != 0)) {
				/*pNewRects[i].right  = exportContext->width;
				pNewRects[i].bottom = exportContext->height;*/
			}
		}
		return oRSSetScissorRects(pThis, NumRects, pNewRects);
	}

	return oRSSetScissorRects(pThis, NumRects, pRects);
}

static HRESULT GetData(
	ID3D11DeviceContext *pThis,
	ID3D11Asynchronous  *pAsync,
	void                *pData,
	UINT                DataSize,
	UINT                GetDataFlags
	) {
	return oGetData(pThis, pAsync, pData, DataSize, GetDataFlags);
}

static void Hook_ClearRenderTargetView(
	ID3D11DeviceContext    *pThis,
	ID3D11RenderTargetView *pRenderTargetView,
	const FLOAT            ColorRGBA[4]
	) {
	//LOG(__func__);
	oClearRenderTargetView(pThis, pRenderTargetView, ColorRGBA);
}

static void Hook_OMSetRenderTargets(
	ID3D11DeviceContext           *pThis,
	UINT                          NumViews,
	ID3D11RenderTargetView *const *ppRenderTargetViews,
	ID3D11DepthStencilView        *pDepthStencilView
	) {
	//	for (int i = 0; i < NumViews; i++) {
	//		if (ppRenderTargetViews[i] == NULL) { continue; }
	//
	//		ComPtr<ID3D11Resource> pResource;
	//		ComPtr<ID3D11Texture2D> pTexture2D;
	//
	//		ppRenderTargetViews[i]->GetResource(pResource.GetAddressOf());
	//		if (SUCCEEDED(pResource.As<ID3D11Texture2D>(&pTexture2D))) {
	//			ComPtr<ID3D11PixelShader> pPixelShader;
	//			pThis->PSGetShader(pPixelShader.GetAddressOf(), NULL, NULL);
	//
	//			D3D11_TEXTURE2D_DESC desc;
	//			pTexture2D->GetDesc(&desc);
	//
	//			D3D11_TEXTURE2D_DESC primaryTexDesc;
	//			pPrimaryRenderTarget->GetDesc(&primaryTexDesc);
	//
	//			if ((pTexture2D != pPrimaryRenderTarget) && (memcmp(&desc, &primaryTexDesc, sizeof(D3D11_TEXTURE2D_DESC)) == 0)) {
	//				LOG("[", i, "/", NumViews,
	//					"] -> w:", desc.Width,
	//					" h:", desc.Height,
	//					" fmt:", conv_dxgi_format_to_string(desc.Format),
	//					" ml:", desc.MipLevels,
	//					" u:", desc.Usage,
	//					std::hex, std::uppercase,
	//					" ps:", (uint)pPixelShader.Get(),
	//					" bf:0x", desc.BindFlags,
	//					" caf:0x", desc.CPUAccessFlags,
	//					" mf:0x", desc.MiscFlags,
	//					" ptr:0x", (uint)ppRenderTargetViews[i],
	//					" isPrimary:", (bool)((pPrimaryRenderTarget != NULL) && (pTexture2D == pPrimaryRenderTarget)),
	//					std::dec, std::nouppercase);
	//
	//
	//				i/*f (pBackbufferRenderTarget == NULL) {
	//					pBackbufferRenderTarget = pTexture2D;
	//				}*/
	///*
	//				if (pTexture2D != pBackbufferRenderTarget) {
	//					return oOMSetRenderTargets(pThis, primaryCount, pRTV, pDSV);
	//				}*/
	//			}
	//
	//
	//			/*if (pTexture2D == pPrimaryRenderTarget) {
	//				pRTV = ppRenderTargetViews;
	//				pDSV = pDepthStencilView;
	//				primaryCount = NumViews;
	//			}*/
	//
	//		}
	//		
	//	}
	//LOG(__func__);
	//return oOMSetRenderTargets(pThis, NumViews, ppRenderTargetViews, pDepthStencilView);

	if ((exportContext != NULL) && isCurrentRenderTargetView(pThis, exportContext->pExportRenderTargetView)) {
		std::lock_guard<std::mutex> sessionLock(mxSession);
		if ((session != NULL) && (session->isCapturing)) {
			// Time to capture rendered frame
			ComPtr<ID3D11ShaderResourceView> pShaderResourceView;
			pThis->VSGetShaderResources(0, 1, pShaderResourceView.GetAddressOf());
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			pShaderResourceView->GetDesc(&desc);
			try {
				NOT_NULL(exportContext, "No export context detected! Cannot capture lossless frame.");

				auto& image_ref = *(exportContext->latestImage);
				LOG_CALL(LL_DBG, DirectX::CaptureTexture(exportContext->pDevice.Get(), exportContext->pDeviceContext.Get(), exportContext->pExportRenderTarget.Get(), image_ref));
				if (exportContext->latestImage->GetImageCount() == 0) {
					LOG(LL_ERR, "There is no image to capture.");
					throw std::exception();
				}
				const DirectX::Image* image = exportContext->latestImage->GetImage(0, 0, 0);
				NOT_NULL(image, "Could not get current frame.");
				NOT_NULL(image->pixels, "Could not get current frame.");

				REQUIRE(session->enqueueVideoFrame(image->pixels, (int)(image->width * image->height * 4), exportContext->pts++), "Failed to enqueue frame");
				exportContext->latestImage->Release();
			} catch (std::exception&) {
				LOG(LL_ERR, "Reading video frame from D3D Device failed.");
				exportContext->latestImage->Release();
				//SafeDelete(session);
				session.reset();
				exportContext.reset();
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
	/*if (mainSwapChain != NULL) {
		DXGI_SWAP_CHAIN_DESC desc;
		REQUIRE(mainSwapChain->GetDesc(&desc), "Failed to get swap chain's describer", std::exception());
		REQUIRE(mainSwapChain->ResizeBuffers(desc.BufferCount, 1920, 1080, DXGI_FORMAT_R8G8B8A8_UNORM, desc.Flags), "Failed to resize swapchain buffers", std::exception());
	}*/
	/*LOG("CreateSWFU: ", std::this_thread::get_id());
	LOG(" rtv:", (uint32_t)createdRTVs[std::this_thread::get_id()]);
	LOG(" dsv:", (uint32_t)createdDSVs[std::this_thread::get_id()]);*/
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
	//NullByteStream* stream = new NullByteStream();
}

static HRESULT Hook_IMFTransform_ProcessMessage(
	IMFTransform     *pThis,
	MFT_MESSAGE_TYPE eMessage,
	ULONG_PTR        ulParam
	) {
	/*if (eMessage == MFT_MESSAGE_NOTIFY_START_OF_STREAM && (session != NULL) && (!session->isVideoContextCreated)) {
		
	} */
	return S_OK;
	//return oIMFTransform_ProcessMessage(pThis, eMessage, ulParam);
}

static HRESULT Hook_IMFTransform_ProcessInput(
	IMFTransform	*pThis,
	DWORD			dwInputStreamID,
	IMFSample		*pSample,
	DWORD			dwFlags
	) {
	ComPtr<IMFMediaType> mediaType;
	pThis->GetInputCurrentType(dwInputStreamID, mediaType.GetAddressOf());
	LOG(LL_NFO, "IMFTransform::ProcessInput: ", GetMediaTypeDescription(mediaType.Get()).c_str());

	return S_OK; // oIMFTransform_ProcessInput(pThis, dwInputStreamID, pSample, dwFlags);
}

static HRESULT Hook_IMFSinkWriter_AddStream(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	) {
	PRE();
	LOG(LL_NFO, "IMFSinkWriter::AddStream: ", GetMediaTypeDescription(pTargetMediaType).c_str());
	POST();

	GUID majorType;
	pTargetMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);

	if (IsEqualGUID(majorType, MFMediaType_Video)) {
		//MFGetAttribute2UINT32asUINT64(pTargetMediaType, MF_MT_FRAME_SIZE, &exportContext->outputWidth, &exportContext->outputHeight);
	}

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
				//LOG(LL_NFO, "MFT_MESSAGE_NOTIFY_START_OF_STREAM");
				//LOG(LL_NFO, "Input media type: ", GetMediaTypeDescription(exportContext->videoMediaType.Get()).c_str());
				//if (!session->isVideoContextCreated) {

				// Create Video Context
				{
					UINT width, height, fps_num, fps_den;
					MFGetAttribute2UINT32asUINT64(exportContext->videoMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
					MFGetAttributeRatio(exportContext->videoMediaType.Get(), MF_MT_FRAME_RATE, &fps_num, &fps_den);

					GUID pixelFormat;
					exportContext->videoMediaType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);

					/*if ((exportContext->width != 0) && (exportContext->height != 0)) {
					width = exportContext->width;
					height = exportContext->height;
					}*/

					//LOG_CALL(session->createVideoContext(width, height, AV_PIX_FMT_BGRA, fps_num, fps_den, AV_PIX_FMT_YUV420P));
					REQUIRE(session->createVideoContext(exportContext->outputWidth, exportContext->outputHeight, "bgra", fps_num, fps_den, Config::instance().videoFmt(), Config::instance().videoEnc(), Config::instance().videoCfg()), "Failed to create video context");
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
						// REQUIRE(session->createAudioContext(numChannels, sampleRate, bitsPerSample, AV_SAMPLE_FMT_S16, blockAlignment), "Failed to create audio context.", std::exception());
						REQUIRE(session->createAudioContext(numChannels, sampleRate, bitsPerSample, AV_SAMPLE_FMT_S16, blockAlignment, Config::instance().audioRate(), Config::instance().audioFmt(), Config::instance().audioEnc(), Config::instance().audioCfg()), "Failed to create audio context.");
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
					std::string output_file = Config::instance().outputDir();

					output_file += "\\XVX-";
					time_t rawtime;
					struct tm timeinfo;
					time(&rawtime);
					localtime_s(&timeinfo, &rawtime);
					strftime(buffer, 128, "%Y%m%d%H%M%S", &timeinfo);
					output_file += buffer;
					output_file += ".mkv";

					std::string filename = std::regex_replace(output_file, std::regex("\\\\+"), "\\");

					LOG(LL_NFO, "Output file: ", filename);

					REQUIRE(session->createFormatContext(filename.c_str()), "Failed to create format context");
				}
				//}
			} catch (std::exception&) {
				//SafeDelete(session);
				session.reset();
				exportContext.reset();
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
	//PRE();
	std::lock_guard<std::mutex> sessionLock(mxSession);
	if ((session != NULL) && (dwStreamIndex == 1)) {
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
			//SafeDelete(session);
			LOG(LL_ERR, ex.what());
			session.reset();
			exportContext.reset();
			if (pBuffer != NULL) {
				pBuffer->Unlock();
			}
		}
	}
	//POST();
	return S_OK; // oIMFSinkWriter_WriteSample(pThis, dwStreamIndex, pSample);
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
	} catch (std::exception&) {
		// Do nothing
	}

	//SafeDelete(session);
	session.reset();
	exportContext.reset();
	POST();
	return S_OK; // return oIMFSinkWriter_Finalize(pThis);
}


void finalize() {
	PRE();
	LOG_CALL(LL_DBG, yr_compiler_destroy(pYrCompiler));
	LOG_CALL(LL_DBG, yr_finalize());
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

static HRESULT Lock(
	IMFMediaBuffer *pThis,
	BYTE  **ppbBuffer,
	DWORD *pcbMaxLength,
	DWORD *pcbCurrentLength
	) {
	//LOG("Lock cl:", pcbCurrentLength == NULL ? 0 : (uint)(*pcbCurrentLength), " ml:", pcbMaxLength == NULL ? 0 : (uint)(*pcbMaxLength));
	return oLock(pThis, ppbBuffer, pcbMaxLength, pcbCurrentLength);
}



//static HMODULE Hook_LoadLibraryA(
//	LPCSTR lpFileName
//	) {
//	static bool once = true;
//	HMODULE result = oLoadLibraryA(lpFileName);
//	LOG(lpFileName);
//	if (once && (_strcmpi(lpFileName, "d3d11.dll") == 0)) {
//		//once = false;
//		//LOG("D3D11.DLL Loaded!!!");
//		//LOG_IF_FAILED(hookNamedFunction("d3d11.dll", "D3D11CreateDevice", &Hook_D3D11CreateDevice, &oD3D11CreateDevice, hkD3D11CreateDevice), "Failed to hook D3D11CreateDevice in d3d11.dll");
//		/*hkD3D11CreateDevice2->SetupHook((BYTE*)&D3D11CreateDevice, (BYTE*)&Hook_D3D11CreateDevice);
//		LOG_IF_FAILED(hkD3D11CreateDevice2->Hook() , "Failed to hook D3D11CreateDevice in d3d11.dll");
//		oD3D11CreateDevice = hkD3D11CreateDevice2->GetOriginal<decltype(&D3D11CreateDevice)>();*/
//		hkD3D11CreateDevice3->SetupHook((BYTE*)&D3D11CreateDevice, (BYTE*)&Hook_D3D11CreateDevice, PLH::VEHHook::VEHMethod::HARDWARE_BP);
//		LOG_IF_FAILED(hkD3D11CreateDevice3->Hook() , "Failed to hook D3D11CreateDevice in d3d11.dll");
//		oD3D11CreateDevice = hkD3D11CreateDevice3->GetOriginal<decltype(&D3D11CreateDevice)>();
//	}
//	return result;
//}

//static void Hook_PSSetShader(
//	ID3D11DeviceContext *pThis,
//	ID3D11PixelShader   *pPixelShader,
//	ID3D11ClassInstance *const *ppClassInstances,
//	UINT                NumClassInstances) {
//	//return;
//	//pThis->PSGetShaderResources()
//	oPSSetShader(pThis, pPixelShader, ppClassInstances, NumClassInstances);
//}

//static void Hook_Draw(
//	ID3D11DeviceContext *pThis,
//	UINT                VertexCount,
//	UINT                StartVertexLocation
//	) {
///*
//	int x;
//
//
//	for (int i = 0; i < 32; i++) {
//		LOG(i, ":", (uint)(*((&x) + i)));
//	}*/
//
//
//
//	/*ComPtr<ID3D11PixelShader> pPixelShader;
//	ComPtr<ID3D11ClassInstance> pClassInstance;
//	UINT numInstances;
//
//	pThis->PSGetShader(pPixelShader.GetAddressOf(), pClassInstance.GetAddressOf(), &numInstances);*/
//	/*if (pClassInstance.Get() == NULL) {
//		LOG("ClassInstance is NULL");
//	} else {
//		char buffer[1024];
//		SIZE_T length;
//		pClassInstance->GetInstanceName(buffer, &length);
//		LOG(std::string(buffer));
//	}*/
//
//	/*ComPtr<ID3D11RasterizerState> pRasterizerState;
//	D3D11_RASTERIZER_DESC desc;
//	pThis->RSGetState(pRasterizerState.GetAddressOf());
//	pRasterizerState->GetDesc(&desc);
//	LOG("cm:", (int)desc.CullMode, " db:", (int)desc.DepthBias, " dbc:", (float)desc.DepthBiasClamp, " fm:", (int)desc.FillMode, " fcc:", desc.FrontCounterClockwise, " se:", desc.ScissorEnable, " ssdb:", (float)desc.SlopeScaledDepthBias, " dce:", desc.DepthClipEnable);*/
//
//	/*if (desc.CullMode == 2) return;*/
//
//	oDraw(pThis, VertexCount, StartVertexLocation);
//}

static HRESULT Hook_Map(
	ID3D11DeviceContext      *pThis,
	ID3D11Resource           *pResource,
	UINT                     Subresource,
	D3D11_MAP                MapType,
	UINT                     MapFlags,
	D3D11_MAPPED_SUBRESOURCE *pMappedResource
	) {

	/*if ((exportContext != NULL) && (exportContext->pExportRenderTarget != NULL)) {
		ComPtr<ID3D11Texture2D> pTexture;
		if (SUCCEEDED(pResource->QueryInterface(pTexture.GetAddressOf()))) {
			if (pTexture == exportContext->pExportRenderTarget) {
				LOG("Mapping the texture here.");
			}
		}
	}*/

	return oMap(pThis, pResource, Subresource, MapType, MapFlags, pMappedResource);
}

static HRESULT Hook_CreateTexture2D(
	ID3D11Device*				 *pThis,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D        **ppTexture2D
	) {
	/*static bool count = 0;
	if ((std::this_thread::get_id() == mainThreadId) && (count++ < 7)) {
		D3D11_TEXTURE2D_DESC* desc = (D3D11_TEXTURE2D_DESC*)pDesc;
		desc->Width = Config::instance().exportResolution().first;
		desc->Height = Config::instance().exportResolution().second;
	}*/

	// Detect export buffer creation
	if (pDesc && (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ) && (std::this_thread::get_id() == mainThreadId)) {
		std::lock_guard<std::mutex> sessionLock(mxSession);
		exportContext.reset();
		//SafeDelete(session);
		session.reset();
		try {
			LOG(LL_NFO, "Creating session...");
			LOG(LL_NFO, " fmt:", conv_dxgi_format_to_string(pDesc->Format),
				" w:", pDesc->Width,
				" h:", pDesc->Height);
			if (Config::instance().isAutoReloadEnabled()) {
				LOG_CALL(LL_DBG,Config::instance().reload());
			}
			session.reset(new Encoder::Session());
			NOT_NULL(session, "Could not create the session");
			exportContext.reset(new ExportContext());
			exportContext->captureRenderTargetViewReference = true;
			exportContext->captureDepthStencilViewReference = true;
			/*exportContext->width = Config::instance().exportResolution().first;
			exportContext->height = Config::instance().exportResolution().second;*/
			/*exportContext->width = 800;
			exportContext->height = 600;*/
			D3D11_TEXTURE2D_DESC desc = *(pDesc);
			/*desc.Width = exportContext->width;
			desc.Height = exportContext->height;*/
			return oCreateTexture2D(pThis, &desc, pInitialData, ppTexture2D);
		} catch (std::exception&) {
			//SafeDelete(session);
			session.reset();
			exportContext.reset();
		}
	}

	return oCreateTexture2D(pThis, pDesc, pInitialData, ppTexture2D);

	/*LOG("CT2D");

	HRESULT result = oCreateTexture2D(pThis, pDesc, pInitialData, ppTexture2D);

	D3D11_TEXTURE2D_DESC desc;
	pPrimaryRenderTarget->GetDesc(&desc);

	if (memcmp(pDesc, &desc, sizeof(D3D11_TEXTURE2D_DESC)) == 0) {
		LOG("TEXTURE WITH SCREEN SIZE CREATED");
	}

	return result;*/
}

//static HRESULT Hook_D3D11CreateDevice(
//	IDXGIAdapter              *pAdapter,
//	D3D_DRIVER_TYPE           DriverType,
//	HMODULE                   Software,
//	UINT                      Flags,
//	const D3D_FEATURE_LEVEL   *pFeatureLevels,
//	UINT                      FeatureLevels,
//	UINT                      SDKVersion,
//	ID3D11Device              **ppDevice,
//	D3D_FEATURE_LEVEL         *pFeatureLevel,
//	ID3D11DeviceContext       **ppImmediateContext
//	) {
//	auto ProtectionObject = hkD3D11CreateDevice3->GetProtectionObject();
//	LOG("D3D11CreateDevice");
//	return oD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
//}



//std::shared_ptr<PLH::VFuncDetour> hkDraw(new PLH::VFuncDetour);
//std::shared_ptr<PLH::VFuncDetour> hkPSSetShader(new PLH::VFuncDetour);
//std::shared_ptr<PLH::IATHook> hkD3D11CreateDevice(new PLH::IATHook);
//std::shared_ptr<PLH::IATHook> hkLoadLibraryA(new PLH::IATHook);
//std::shared_ptr<PLH::IATHook> hkHook_RtlEnterCriticalSection(new PLH::IATHook);
//std::shared_ptr<PLH::X64Detour> hkD3D11CreateDevice2(new PLH::X64Detour);
//std::shared_ptr<PLH::VEHHook> hkD3D11CreateDevice3(new PLH::VEHHook);

//typedef void (*tRtlEnterCriticalSection)(
//	LPCRITICAL_SECTION lpCriticalSection
//	);

//typedef HMODULE (*tLoadLibraryA)(
//	LPCSTR lpFileName
//	);


//typedef void (*tPSSetShader)(
//	ID3D11DeviceContext *pThis,
//	ID3D11PixelShader   *pPixelShader,
//	ID3D11ClassInstance *const *ppClassInstances,
//	UINT                NumClassInstances);

//typedef void (*tDraw)(
//	ID3D11DeviceContext *pThis,
//	UINT                VertexCount,
//	UINT                StartVertexLocation
//	);


//typedef HRESULT (*tD3D11CreateDevice)(
//	IDXGIAdapter              *pAdapter,
//	D3D_DRIVER_TYPE           DriverType,
//	HMODULE                   Software,
//	UINT                      Flags,
//	const D3D_FEATURE_LEVEL   *pFeatureLevels,
//	UINT                      FeatureLevels,
//	UINT                      SDKVersion,
//	ID3D11Device              **ppDevice,
//	D3D_FEATURE_LEVEL         *pFeatureLevel,
//	ID3D11DeviceContext       **ppImmediateContext
//	);

//decltype(&D3D11CreateDevice) oD3D11CreateDevice;

//tLoadLibraryA oLoadLibraryA;
//tPSSetShader oPSSetShader;
//tDraw oDraw;
//tRtlEnterCriticalSection oRtlEnterCriticalSection;
//tMap oMap;

//static void Hook_RtlEnterCriticalSection(
//	LPCRITICAL_SECTION lpCriticalSection
//	);

//static HMODULE Hook_LoadLibraryA(
//	LPCSTR lpFileName
//	);




//static void Hook_PSSetShader(
//	ID3D11DeviceContext *pThis,
//	ID3D11PixelShader   *pPixelShader,
//	ID3D11ClassInstance *const *ppClassInstances,
//	UINT                NumClassInstances);

//static void Hook_Draw(
//	ID3D11DeviceContext *pThis,
//	UINT                VertexCount,
//	UINT                StartVertexLocation
//	);

//static HRESULT Hook_D3D11CreateDevice(
//	IDXGIAdapter              *pAdapter,
//	D3D_DRIVER_TYPE           DriverType,
//	HMODULE                   Software,
//	UINT                      Flags,
//	const D3D_FEATURE_LEVEL   *pFeatureLevels,
//	UINT                      FeatureLevels,
//	UINT                      SDKVersion,
//	ID3D11Device              **ppDevice,
//	D3D_FEATURE_LEVEL         *pFeatureLevel,
//	ID3D11DeviceContext       **ppImmediateContext
//	);