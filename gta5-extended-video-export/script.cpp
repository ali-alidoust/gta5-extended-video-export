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
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_AddStream(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_SetInputMediaType(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_WriteSample(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkIMFSinkWriter_Finalize(new PLH::VFuncDetour);
	std::shared_ptr<PLH::VFuncDetour> hkOMSetRenderTargets(new PLH::VFuncDetour);
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
tGetRenderTimeBase oGetRenderTimeBase;
tCreateTexture oCreateTexture;
tDraw oDraw;

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

			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 13, &Draw, &oDraw, hkDraw), "Failed to hook ID3DDeviceContext::Draw");
			REQUIRE(hookVirtualFunction(pDeviceContext.Get(), 33, &Hook_OMSetRenderTargets, &oOMSetRenderTargets, hkOMSetRenderTargets), "Failed to hook ID3DDeviceContext::OMSetRenderTargets");
			ComPtr<IDXGIDevice> pDXGIDevice;
			REQUIRE(pDevice.As(&pDXGIDevice), "Failed to get IDXGIDevice from ID3D11Device");
			
			ComPtr<IDXGIAdapter> pDXGIAdapter;
			REQUIRE(pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)pDXGIAdapter.GetAddressOf()), "Failed to get IDXGIAdapter");
			REQUIRE(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)pDXGIFactory.GetAddressOf()), "Failed to get IDXGIFactory");

		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}
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
		void* pCreateTexture = NULL;
		void* pLinearizeTexture = NULL;
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

static void Hook_OMSetRenderTargets(
	ID3D11DeviceContext           *pThis,
	UINT                          NumViews,
	ID3D11RenderTargetView *const *ppRenderTargetViews,
	ID3D11DepthStencilView        *pDepthStencilView
	) {

	if (::exportContext) {
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

					REQUIRE(session->createFormatContext(filename.c_str(), exrOutputPath, config::format_cfg), "Failed to create format context");
				}
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

static float Detour_GetRenderTimeBase(int64_t choice) {
	std::pair<int32_t, int32_t> fps = config::fps;
	float result = 1000.0f * (float)fps.second / ((float)fps.first * ((float)config::motion_blur_samples + 1));
	LOG(LL_NFO, "Time step: ", result);
	return result;
}

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
			pDevice->CreateTexture2D(&desc, NULL, pStencilTexture.GetAddressOf());
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
			LOG_CALL(LL_DBG, pDevice->CreateTexture2D(&desc, NULL, pLinearDepthTexture.GetAddressOf()));
		} else if (std::string("BackBuffer").compare(name) == 0) {
			pGameBackBufferResolved = nullptr;
			pGameDepthBuffer = nullptr;
			pGameDepthBufferQuarter = nullptr;
			pGameDepthBufferQuarterLinear = nullptr;
			pGameDepthBufferResolved = nullptr;
			pGameEdgeCopy = nullptr;
			pGameGBuffer0 = nullptr;

			pGameBackBuffer = pTexture;
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