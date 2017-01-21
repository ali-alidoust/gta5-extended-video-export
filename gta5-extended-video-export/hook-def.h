#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <dxgi.h>
#include <d3d11.h>
//#include <xaudio2.h>

//static HRESULT SubmitSourceBuffer(
//	IXAudio2SourceVoice      *pThis,
//	const XAUDIO2_BUFFER     *pBuffer,
//	const XAUDIO2_BUFFER_WMA *pBufferWMA
//	);
//
//static HRESULT CreateSourceVoice(
//	      IXAudio2              *pThis,
//	      IXAudio2SourceVoice   **ppSourceVoice,
//	const WAVEFORMATEX          *pSourceFormat,
//	      UINT32                Flags,
//	      float                 MaxFrequencyRatio,
//	      IXAudio2VoiceCallback *pCallback,
//	const XAUDIO2_VOICE_SENDS   *pSendList,
//	const XAUDIO2_EFFECT_CHAIN  *pEffectChain
//	);

static void Draw(
	ID3D11DeviceContext *pThis,
	UINT VertexCount,
	UINT StartVertexLocation
	);

static void DispatchIndirect(
	ID3D11DeviceContext* pThis,
	ID3D11Buffer *pBufferForArgs,
	UINT         AlignedByteOffsetForArgs
	);

static void Dispatch(
	ID3D11DeviceContext* pThis,
	UINT ThreadGroupCountX,
	UINT ThreadGroupCountY,
	UINT ThreadGroupCountZ
	);

static void ClearState(
	ID3D11DeviceContext *pThis
	);

static void VSSetConstantBuffers(
	ID3D11DeviceContext *pThis,
	UINT                StartSlot,
	UINT                NumBuffers,
	ID3D11Buffer *const *ppConstantBuffers
	);

static HRESULT Hook_CreateRenderTargetView(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView        **ppRTView
	);

static HRESULT Hook_CreateDepthStencilView(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	ID3D11DepthStencilView        **ppDepthStencilView
	);

static void RSSetViewports(
	ID3D11DeviceContext  *pThis,
	UINT           NumViewports,
	const D3D11_VIEWPORT *pViewports
	);

static void RSSetScissorRects(
	ID3D11DeviceContext *pThis,
	UINT          NumRects,
	const D3D11_RECT *  pRects
	);

static HRESULT Hook_CreateTexture2D(
	ID3D11Device 				 *pThis,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D        **ppTexture2D
	);

static void Hook_OMSetRenderTargets(
	ID3D11DeviceContext           *pThis,
	UINT                          NumViews,
	ID3D11RenderTargetView *const *ppRenderTargetViews,
	ID3D11DepthStencilView        *pDepthStencilView
	);

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

//typedef HRESULT (*tSubmitSourceBuffer)(
//	IXAudio2SourceVoice      *pThis,
//	const XAUDIO2_BUFFER     *pBuffer,
//	const XAUDIO2_BUFFER_WMA *pBufferWMA
//	);
//
//typedef HRESULT (*tCreateSourceVoice)(
//	IXAudio2                    *pThis,
//	IXAudio2SourceVoice         **ppSourceVoice,
//	const WAVEFORMATEX          *pSourceFormat,
//	UINT32                      Flags,
//	float                       MaxFrequencyRatio,
//	IXAudio2VoiceCallback       *pCallback,
//	const XAUDIO2_VOICE_SENDS   *pSendList,
//	const XAUDIO2_EFFECT_CHAIN  *pEffectChain
//	);

typedef void (*tDraw)(
	ID3D11DeviceContext *pThis,
	UINT VertexCount,
	UINT StartVertexLocation
	);

typedef void (*tDispatchIndirect)(
	ID3D11DeviceContext* pThis,
	ID3D11Buffer *pBufferForArgs,
	UINT         AlignedByteOffsetForArgs
	);

typedef void (*tDispatch)(
	ID3D11DeviceContext* pThis,
	UINT ThreadGroupCountX,
	UINT ThreadGroupCountY,
	UINT ThreadGroupCountZ
	);

typedef void (*tClearState)(
	ID3D11DeviceContext *pThis
	);

typedef void (*tVSSetConstantBuffers)(
	ID3D11DeviceContext *pThis,
	UINT                StartSlot,
	UINT                NumBuffers,
	ID3D11Buffer *const *ppConstantBuffers
	);

typedef HRESULT(*tCreateTexture2D)(
	ID3D11Device 				 *pThis,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D        **ppTexture2D
	);

typedef void(*tOMSetRenderTargets)(
	ID3D11DeviceContext           *pThis,
	UINT                          NumViews,
	ID3D11RenderTargetView *const *ppRenderTargetViews,
	ID3D11DepthStencilView        *pDepthStencilView
	);

typedef HRESULT(*tMFCreateSinkWriterFromURL)(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	);

typedef HRESULT(*tCoCreateInstance)(
	REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv
	);

typedef HRESULT(*tCreateRenderTargetView)(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView        **ppRTView
	);

typedef HRESULT(*tCreateDepthStencilView)(
	ID3D11Device                  *pThis,
	ID3D11Resource                *pResource,
	const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	ID3D11DepthStencilView        **ppDepthStencilView
	);

typedef void(*tRSSetViewports)(
	ID3D11DeviceContext  *pThis,
	UINT                 NumViewports,
	const D3D11_VIEWPORT *pViewports
	);

typedef void(*tRSSetScissorRects)(
	ID3D11DeviceContext *pThis,
	UINT          NumRects,
	const D3D11_RECT *  pRects
	);

typedef HRESULT(*tIMFSinkWriter_AddStream)(
	IMFSinkWriter *pThis,
	IMFMediaType  *pTargetMediaType,
	DWORD         *pdwStreamIndex
	);

typedef HRESULT(*tIMFSinkWriter_SetInputMediaType)(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters
	);

typedef HRESULT(*tIMFSinkWriter_WriteSample)(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample
	);

typedef HRESULT(*tIMFSinkWriter_Finalize)(
	IMFSinkWriter *pThis
	);