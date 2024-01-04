#pragma once

#include "hooks.h"

#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfreadwrite.h>

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, Draw, 13, void, //
                   UINT vertex_count,                   //
                   UINT start_vertex_location);         //

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, DrawIndexed, 12, void, //
                   UINT index_count,                           //
                   UINT start_index_location,                  //
                   INT base_vertex_location);                  //

DEFINE_MEMBER_HOOK(IDXGISwapChain, Present, 8, HRESULT, //
                   UINT sync_interval,                  //
                   UINT flags);                         //

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, OMSetRenderTargets, 33, void,     //
                   UINT num_views,                                        //
                   ID3D11RenderTargetView* const* pp_render_target_views, //
                   ID3D11DepthStencilView* p_depth_stencil_view);         //

DEFINE_MEMBER_HOOK(IMFSinkWriter, AddStream, 3, HRESULT, //
                   IMFMediaType* p_target_media_type,    //
                   DWORD* pdw_stream_index);             //

DEFINE_MEMBER_HOOK(IMFSinkWriter, SetInputMediaType, 4, HRESULT, //
                   DWORD dw_stream_index,                        //
                   IMFMediaType* p_input_media_type,             //
                   IMFAttributes* p_encoding_parameters);        //

DEFINE_MEMBER_HOOK(IMFSinkWriter, WriteSample, 6, HRESULT, //
                   DWORD dw_stream_index,                  //
                   IMFSample* p_sample);                   //

DEFINE_MEMBER_HOOK(IMFSinkWriter, Finalize, 11, HRESULT);

DEFINE_NAMED_IMPORT_HOOK("mfreadwrite.dll", MFCreateSinkWriterFromURL, HRESULT, //
                         LPCWSTR pwsz_output_url,                               //
                         IMFByteStream* p_byte_stream,                          //
                         IMFAttributes* p_attributes,                           //
                         IMFSinkWriter** pp_sink_writer);                       //

DEFINE_NAMED_EXPORT_HOOK("d3d11.dll", D3D11CreateDeviceAndSwapChain, HRESULT, //
                         IDXGIAdapter* p_adapter,                             //
                         D3D_DRIVER_TYPE driver_type,                         //
                         HMODULE software,                                    //
                         UINT flags,                                          //
                         const D3D_FEATURE_LEVEL* p_feature_levels,           //
                         UINT feature_levels,                                 //
                         UINT sdk_version,                                    //
                         const DXGI_SWAP_CHAIN_DESC* p_swap_chain_desc,       //
                         IDXGISwapChain** pp_swap_chain,                      //
                         ID3D11Device** pp_device,                            //
                         D3D_FEATURE_LEVEL* p_feature_level,                  //
                         ID3D11DeviceContext** pp_immediate_context);         //

DEFINE_NAMED_IMPORT_HOOK("kernel32.dll", LoadLibraryW, HMODULE, //
                         LPCWSTR lp_lib_file_name);             //

DEFINE_NAMED_IMPORT_HOOK("kernel32.dll", LoadLibraryA, HMODULE, //
                         LPCSTR lp_lib_file_name);              //

DEFINE_X64_HOOK(GetRenderTimeBase, float, //
                int64_t choice);          //

DEFINE_X64_HOOK(CreateTexture, void*, //
                void* rcx,            //
                char* name,           //
                uint32_t r8d,         //
                uint32_t width,       //
                uint32_t height,      //
                uint32_t format,      //
                void* rsp30);         //

DEFINE_X64_HOOK(CreateThread, HANDLE, //
                void* p_func,         //
                void* p_params,       //
                int32_t r8d,          //
                int32_t r9d,          //
                void* rsp20,          //
                int32_t rsp28,        //
                char* name);          //

DEFINE_X64_HOOK(CreateExportContext, uint8_t, //
                void* p_context,              //
                uint32_t width,               //
                uint32_t height,              //
                void* r9d);                   //