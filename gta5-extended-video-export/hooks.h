// ReSharper disable CppInconsistentNaming
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#pragma once
#define NOMINMAX
#include <Windows.h>

#include "logger.h"

#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/PE/IatHook.hpp>
#include <polyhook2/Virtuals/VFuncSwapHook.hpp>
#include <polyhook2/ZydisDisassembler.hpp>

#include <d3d11.h>

#include <mfapi.h>
#include <mfreadwrite.h>

struct MemberHookInfoStruct {
    explicit MemberHookInfoStruct(const uint16_t index) : index(index) {}

    uint16_t index;
    std::shared_ptr<PLH::VFuncSwapHook> hook;
};

template <class CLASS_TYPE, class FUNC_TYPE>
HRESULT hookMemberFunction(CLASS_TYPE* pInstance, FUNC_TYPE hookFunc, FUNC_TYPE* originalFunc,
                           MemberHookInfoStruct& info) {
    if (info.hook.get() != nullptr) {
        return S_OK;
    }

    const PLH::VFuncMap hookMap = {{info.index, reinterpret_cast<uint64_t>(hookFunc)}};
    PLH::VFuncMap originalFunctions;
    const auto newHook = new PLH::VFuncSwapHook(reinterpret_cast<uint64_t>(pInstance), hookMap, &originalFunctions);
    if (!newHook->hook()) {
        delete newHook;
        LOG(LL_ERR, "Failed to hook function!");
        return E_FAIL;
    }

    *originalFunc = ForceCast<FUNC_TYPE, uint64_t>(originalFunctions[info.index]);
    info.hook.reset(newHook);
    return S_OK;
}

template <class FUNC_TYPE>
HRESULT hookNamedFunction(const std::string& dllName, const std::string& apiName, LPVOID hookFunc,
                          FUNC_TYPE* originalFunc, std::shared_ptr<PLH::IatHook>& IatHook) {

    if (IatHook.get() != nullptr) {
        return S_OK;
    }

    const auto newHook = new PLH::IatHook(dllName, apiName, reinterpret_cast<uint64_t>(hookFunc),
                                          reinterpret_cast<uint64_t*>(originalFunc), L"");
    if (!newHook->hook()) {
        *originalFunc = nullptr;
        delete newHook;
        LOG(LL_ERR, "Failed to hook function!");
        return E_FAIL;
    }

    IatHook.reset(newHook);
    return S_OK;
}

template <class FUNC_TYPE>
HRESULT hookX64Function(uint64_t func, void* hookFunc, FUNC_TYPE* originalFunc,
                        std::shared_ptr<PLH::x64Detour>& x64Detour) {
    if (x64Detour.get() != nullptr) {
        return S_OK;
    }

    static PLH::ZydisDisassembler Disassembler(PLH::Mode::x64);

    const auto newHook = new PLH::x64Detour(func, reinterpret_cast<uint64_t>(hookFunc), reinterpret_cast<uint64_t*>(originalFunc));

    if (!newHook->hook()) {
        *originalFunc = nullptr;
        delete newHook;
        LOG(LL_ERR, "Failed to hook x64 function.");
        return E_FAIL;
    }

    x64Detour.reset(newHook);
    return S_OK;
}

#define DEFINE_MEMBER_HOOK(BASE_CLASS, METHOD_NAME, VFUNC_INDEX, RETURN_TYPE, ...)                                     \
    namespace BASE_CLASS##Hooks {                                                                                      \
        namespace METHOD_NAME {                                                                                        \
        RETURN_TYPE Implementation(BASE_CLASS* pThis, __VA_ARGS__);                                             \
        typedef RETURN_TYPE (*##Type)(BASE_CLASS * pThis, __VA_ARGS__);                                                \
        Type OriginalFunc = nullptr;                                                                                   \
        MemberHookInfoStruct Info(VFUNC_INDEX);                                                                        \
        }                                                                                                              \
    }

#define DEFINE_NAMED_IMPORT_HOOK(DLL_NAME_STRING, FUNCTION_NAME, RETURN_TYPE, ...)                                     \
    namespace Import##Hooks {                                                                                          \
        namespace FUNCTION_NAME {                                                                                      \
        RETURN_TYPE Implementation(__VA_ARGS__);                                                                \
        typedef RETURN_TYPE (*##Type)(__VA_ARGS__);                                                                    \
        Type OriginalFunc = nullptr;                                                                                   \
        std::shared_ptr<PLH::IatHook> Hook;                                                                            \
        }                                                                                                              \
    }

#define DEFINE_X64_HOOK(FUNCTION_NAME, RETURN_TYPE, ...)                                                               \
    namespace Game##Hooks {                                                                                            \
        namespace FUNCTION_NAME {                                                                                      \
        RETURN_TYPE Implementation(__VA_ARGS__);                                                                \
        typedef RETURN_TYPE (*##Type)(__VA_ARGS__);                                                                    \
        Type OriginalFunc = nullptr;                                                                                   \
        std::shared_ptr<PLH::x64Detour> Hook;                                                                          \
        }                                                                                                              \
    }

#define PERFORM_MEMBER_HOOK_REQUIRED(BASE_CLASS, METHOD_NAME, P_INSTANCE)                                              \
    REQUIRE(hookMemberFunction(P_INSTANCE, BASE_CLASS##Hooks::METHOD_NAME::Implementation,                             \
                               &BASE_CLASS##Hooks::METHOD_NAME::OriginalFunc, BASE_CLASS##Hooks::METHOD_NAME::Info),   \
            "Failed to hook " #BASE_CLASS "::" #METHOD_NAME)

#define PERFORM_NAMED_IMPORT_HOOK_REQUIRED(DLL_NAME_STRING, FUNCTION_NAME)                                             \
    REQUIRE(hookNamedFunction(DLL_NAME_STRING, #FUNCTION_NAME, ImportHooks::FUNCTION_NAME::Implementation,             \
                              &ImportHooks::FUNCTION_NAME::OriginalFunc, ImportHooks::FUNCTION_NAME::Hook),            \
            "Failed to hook " #FUNCTION_NAME "in " DLL_NAME_STRING)

#define PERFORM_X64_HOOK_REQUIRED(FUNCTION_NAME, ADDRESS)                                                              \
    REQUIRE(hookX64Function(ADDRESS, GameHooks::FUNCTION_NAME::Implementation,                                         \
                            &GameHooks::FUNCTION_NAME::OriginalFunc, GameHooks::FUNCTION_NAME::Hook),                  \
            "Failed to hook " #FUNCTION_NAME)

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, Draw, 13, void, //
                   UINT VertexCount,                    //
                   UINT StartVertexLocation);           //

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, DrawIndexed, 12, void, //
                   UINT IndexCount,                            //
                   UINT StartIndexLocation,                    //
                   INT BaseVertexLocation);

DEFINE_MEMBER_HOOK(IDXGISwapChain, Present, 8, HRESULT, //
                   UINT SyncInterval,                   //
                   UINT Flags);                         //

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, OMSetRenderTargets, 33, void,  //
                   UINT NumViews,                                      //
                   ID3D11RenderTargetView* const* ppRenderTargetViews, //
                   ID3D11DepthStencilView* pDepthStencilView);         //

DEFINE_MEMBER_HOOK(IMFSinkWriter, AddStream, 3, HRESULT, //
                   IMFMediaType* pTargetMediaType,       //
                   DWORD* pdwStreamIndex);               //

DEFINE_MEMBER_HOOK(IMFSinkWriter, SetInputMediaType, 4, HRESULT, //
                   DWORD dwStreamIndex,                          //
                   IMFMediaType* pInputMediaType,                //
                   IMFAttributes* pEncodingParameters);          //

DEFINE_MEMBER_HOOK(IMFSinkWriter, WriteSample, 6, HRESULT, //
                   DWORD dwStreamIndex,                    //
                   IMFSample* pSample);                    //

DEFINE_MEMBER_HOOK(IMFSinkWriter, Finalize, 11, HRESULT);

DEFINE_NAMED_IMPORT_HOOK("mfreadwrite.dll", MFCreateSinkWriterFromURL,
                         HRESULT,                       //
                         LPCWSTR pwszOutputURL,         //
                         IMFByteStream* pByteStream,    //
                         IMFAttributes* pAttributes,    //
                         IMFSinkWriter** ppSinkWriter); //

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
                void* pFunc,          //
                void* pParams,        //
                int32_t r8d,          //
                int32_t r9d,          //
                void* rsp20,          //
                int32_t rsp28,        //
                char* name);          //

DEFINE_X64_HOOK(CreateExportContext, uint8_t, //
                void* pContext,               //
                uint32_t width,               //
                uint32_t height,              //
                void* r9d);                   //