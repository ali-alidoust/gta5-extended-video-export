#pragma once

#include "hooks.h"

#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfreadwrite.h>

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