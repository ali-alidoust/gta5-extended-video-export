#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <dxgi.h>
#include <d3d11.h>

static HRESULT CreateObjectFromURL(
	IMFSourceResolver *pThis,
	LPCWSTR           pwszURL,
	DWORD             dwFlags,
	IPropertyStore    *pProps,
	MF_OBJECT_TYPE    *pObjectType,
	IUnknown          **ppObject
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

static HRESULT GetData(
	ID3D11DeviceContext *pThis,
	ID3D11Asynchronous  *pAsync,
	void                *pData,
	UINT                DataSize,
	UINT                GetDataFlags
	);

static HRESULT Hook_CreateTexture2D(
	ID3D11Device*				 *pThis,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D        **ppTexture2D
	);

static void CopyResource(
	ID3D11DeviceContext *pThis,
	ID3D11Resource      *pDstResource,
	ID3D11Resource      *pSrcResource
	);

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
	);


static void Hook_ClearRenderTargetView(
	ID3D11DeviceContext    *pThis,
	ID3D11RenderTargetView *pRenderTargetView,
	const FLOAT            ColorRGBA[4]
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

static HRESULT Lock(
	IMFMediaBuffer *pThis,
	BYTE  **ppbBuffer,
	DWORD *pcbMaxLength,
	DWORD *pcbCurrentLength
	);

static HRESULT Hook_Map(
	ID3D11DeviceContext      *pThis,
	ID3D11Resource           *pResource,
	UINT                     Subresource,
	D3D11_MAP                MapType,
	UINT                     MapFlags,
	D3D11_MAPPED_SUBRESOURCE *pMappedResource
	);


typedef HRESULT(*tCreateObjectFromURL)(
	IMFSourceResolver *pThis,
	LPCWSTR           pwszURL,
	DWORD             dwFlags,
	IPropertyStore    *pProps,
	MF_OBJECT_TYPE    *pObjectType,
	IUnknown          **ppObject
	);


typedef HRESULT(*tCreateTexture2D)(
	ID3D11Device*				 *pThis,
	const D3D11_TEXTURE2D_DESC   *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
	ID3D11Texture2D        **ppTexture2D
	);

typedef void(*tCopyResource)(
	ID3D11DeviceContext *pThis,
	ID3D11Resource      *pDstResource,
	ID3D11Resource      *pSrcResource
	);

typedef void(*tCopySubresourceRegion)(
	ID3D11DeviceContext  *pThis,
	ID3D11Resource       *pDstResource,
	UINT                 DstSubresource,
	UINT                 DstX,
	UINT                 DstY,
	UINT                 DstZ,
	ID3D11Resource       *pSrcResource,
	UINT                 SrcSubresource,
	const D3D11_BOX      *pSrcBox
	);

typedef HRESULT(*tMap)(
	ID3D11DeviceContext      *pThis,
	ID3D11Resource           *pResource,
	UINT                     Subresource,
	D3D11_MAP                MapType,
	UINT                     MapFlags,
	D3D11_MAPPED_SUBRESOURCE *pMappedResource
	);

typedef void(*tClearRenderTargetView)(
	ID3D11DeviceContext    *pThis,
	ID3D11RenderTargetView *pRenderTargetView,
	const FLOAT            ColorRGBA[4]
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

typedef HRESULT(*tGetData)(
	ID3D11DeviceContext *pThis,
	ID3D11Asynchronous  *pAsync,
	void                *pData,
	UINT                DataSize,
	UINT                GetDataFlags
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

typedef HRESULT(*tLock)(
	IMFMediaBuffer *pThis,
	BYTE  **ppbBuffer,
	DWORD *pcbMaxLength,
	DWORD *pcbCurrentLength
	);