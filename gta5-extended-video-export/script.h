#pragma once

#define NOMINMAX

#include <d3d11.h>
#include <dxgi.h>
#include <future>
#include <queue>
#include <wrl/client.h>

#include "reshade.hpp"

using namespace Microsoft::WRL;

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

template <class B, class A> B ForceCast(A a) {
    union {
        A a;
        B b;
    } x;
    x.a = a;
    return x.b;
}

namespace eve {
void initialize();
void ScriptMain();
void OnPresent(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain,
               const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count,
               const reshade::api::rect* dirty_rects);

bool OnDraw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
            uint32_t first_instance);

bool OnDrawIndexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count,
                   uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);

void OnBindRenderTargets(reshade::api::command_list* cmd_list, uint32_t count,
                         const reshade::api::resource_view* render_targets,
                         reshade::api::resource_view depth_stencil);

void OnInitDevice(reshade::api::device* device);
void OnInitSwapChain(reshade::api::swapchain* swapchain);

// void OnPresent(IDXGISwapChain* p_swap_chain);
void PresentCallback(void* p_swap_chain);

void finalize();
static void PrepareDeferredContext(ID3D11Device* p_device);
static ComPtr<ID3D11Texture2D> divideBuffer(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext,
                                            uint32_t k);
static void drawAdditive(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext,
                         ComPtr<ID3D11Texture2D> pSource);
} // namespace eve
