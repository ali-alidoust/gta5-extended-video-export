// ReSharper disable CppInconsistentNaming
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#pragma once

#include <Windows.h>

#include "logger.h"

#include <polyhook2/ADisassembler.hpp>
#include <polyhook2/CapstoneDisassembler.hpp>
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/PE/IatHook.hpp>
#include <polyhook2/Virtuals/VFuncSwapHook.hpp>

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

    const auto newHook = new PLH::IatHook(dllName, apiName, reinterpret_cast<uint64_t>(hookFunc), reinterpret_cast<uint64_t*>(originalFunc), L"");
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

    static PLH::CapstoneDisassembler Disassembler(PLH::Mode::x64);

    auto newHook = new PLH::x64Detour(func, (uint64_t)hookFunc, (uint64_t*)originalFunc, Disassembler);

    if (!newHook->hook()) {
        *originalFunc = nullptr;
        delete newHook;
        LOG(LL_ERR, "Failed to hook x64 function.");
        // LOG(LL_ERR, X64Detour_ex->GetLastError().GetString());
        return E_FAIL;
    }

    x64Detour.reset(newHook);
    return S_OK;
}

//template <class Derived, class BaseClass, int16_t VirtualFunctionIndex, typename ReturnType, typename... Parameters>
//class MemberHook {
//  public:
//    static HRESULT PerformHook(BaseClass* pInstance) {
//        if (Hook != nullptr) {
//            return S_OK;
//        }
//
//        const PLH::VFuncMap hookMap = {{VirtualFunctionIndex, reinterpret_cast<uint64_t>(CallImplementation)}};
//        PLH::VFuncMap originalFunctions;
//        const auto newHook = new PLH::VFuncSwapHook(reinterpret_cast<uint64_t>(pInstance), hookMap, &originalFunctions);
//        if (!newHook->hook()) {
//            delete newHook;
//            LOG(LL_ERR, "Failed to hook function!");
//            return E_FAIL;
//        }
//        OriginalFunctionPointer = reinterpret_cast<Type>(originalFunctions[VirtualFunctionIndex]);
//        Hook.reset(newHook);
//        return S_OK;
//    }
//
//    static ReturnType Original(BaseClass* pThis, Parameters... params) {
//        if (OriginalFunctionPointer == nullptr) {
//            throw std::logic_error("Invalid original function pointer.");
//        }
//        return GetInstance().OriginalFunctionPointer(pThis, params...);
//    }
//
//    virtual ~MemberHook() {
//        if (Hook) {
//            Hook.get()->unHook();
//            Hook.reset();
//        }
//    }
//    MemberHook() = default;
//    MemberHook(const MemberHook&) = delete;
//    MemberHook& operator=(const MemberHook&) = delete;
//    MemberHook(MemberHook&&) = delete;
//    MemberHook& operator=(MemberHook&&) = delete;
//
//  protected:
//    static std::shared_ptr<PLH::VFuncSwapHook> Hook;
//
//    virtual ReturnType Implementation(BaseClass* pThis, Parameters...) = 0;
//
//    static Derived& GetInstance() {
//        static Derived instance;
//        return instance;
//    }
//
//    static ReturnType CallImplementation(BaseClass* pThis, Parameters... params) {
//        return GetInstance().Implementation(pThis, params...);
//    }
//
//    typedef ReturnType (*Type)(BaseClass* pThis, Parameters...);
//    inline static Type OriginalFunctionPointer = nullptr;
//};

//namespace ID3D11DeviceContextHooks3 {
//class Draw final : public MemberHook<Draw, ID3D11DeviceContext, 13, void, //
//                                     UINT,                                // VertexCount
//                                     UINT                                 // StartVertexLocation
//                                     > {
//  protected:
//    void Implementation(ID3D11DeviceContext* pThis, unsigned, unsigned) override;
//};
//} // namespace ID3D11DeviceContextHooks3

// template <class BaseClass, int16_t VirtualFunctionIndex, typename ReturnType, typename... Parameters>
// static MemberHook<BaseClass, VirtualFunctionIndex, ReturnType, Parameters...> *CreateMemberHook() {
//    const auto result = new MemberHook<BaseClass, VirtualFunctionIndex, ReturnType, Parameters...>{};
//    return result;
//}
//
// namespace ID3D11DeviceContext2 {
// const auto Draw = CreateMemberHook< //
//    ID3D11DeviceContext, 13, void,  //
//    UINT,                           // VertexCount
//    UINT                            // StartVertexLocation
//    >();
//}
//
// template <class BaseClass, typename ReturnType, int16_t VirtualFunctionIndex, typename... Parameters>
// class MemberHook2 {
//  public:
//    static ReturnType Implementation(BaseClass *pThis, Parameters...) = delete;
//    static typedef ReturnType (*Type)(BaseClass *pThis, Parameters...);
//    static Type OriginalFunc = nullptr;
//    static std::shared_ptr<PLH::VFuncSwapHook> Hook;
//
//    static HRESULT PerformHook(BaseClass *pInstance) {
//        if (Hook.get() != nullptr) {
//            return S_OK;
//        }
//
//        const PLH::VFuncMap hookMap = {{VirtualFunctionIndex, static_cast<uint64_t>(Implementation)}};
//        PLH::VFuncMap originalFunctions;
//        const auto newHook = new PLH::VFuncSwapHook(static_cast<uint64_t>(pInstance), hookMap, &originalFunctions);
//        if (!newHook->hook()) {
//            delete newHook;
//            LOG(LL_ERR, "Failed to hook function!");
//            return E_FAIL;
//        }
//
//        OriginalFunc = ForceCast<Type, uint64_t>(originalFunctions[VirtualFunctionIndex]);
//        Hook.reset(newHook);
//        return S_OK;
//    }
//};

#define DEFINE_MEMBER_HOOK(BASE_CLASS, METHOD_NAME, VFUNC_INDEX, RETURN_TYPE, ...)                                     \
    namespace BASE_CLASS##Hooks {                                                                                      \
        namespace METHOD_NAME {                                                                                        \
        static RETURN_TYPE Implementation(BASE_CLASS* pThis, __VA_ARGS__);                                             \
        typedef RETURN_TYPE (*##Type)(BASE_CLASS * pThis, __VA_ARGS__);                                                \
        Type OriginalFunc = nullptr;                                                                                   \
        MemberHookInfoStruct Info(VFUNC_INDEX);                                                                        \
        }                                                                                                              \
    }

#define DEFINE_NAMED_IMPORT_HOOK(DLL_NAME_STRING, FUNCTION_NAME, RETURN_TYPE, ...)                                     \
    namespace Import##Hooks {                                                                                          \
        namespace FUNCTION_NAME {                                                                                      \
        static RETURN_TYPE Implementation(__VA_ARGS__);                                                                \
        typedef RETURN_TYPE (*##Type)(__VA_ARGS__);                                                                    \
        Type OriginalFunc = nullptr;                                                                                   \
        std::shared_ptr<PLH::IatHook> Hook;                                                                            \
        }                                                                                                              \
    }

#define DEFINE_X64_HOOK(FUNCTION_NAME, RETURN_TYPE, ...)                                                               \
    namespace Game##Hooks {                                                                                            \
        namespace FUNCTION_NAME {                                                                                      \
        static RETURN_TYPE Implementation(__VA_ARGS__);                                                                \
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

DEFINE_MEMBER_HOOK(IDXGISwapChain, Present, 8, HRESULT, //
                   UINT SyncInterval,                   //
                   UINT Flags);                         //

DEFINE_MEMBER_HOOK(ID3D11DeviceContext, OMSetRenderTargets, 33, void,  //
                   UINT NumViews,                                      //
                   ID3D11RenderTargetView* const* ppRenderTargetViews, //
                   ID3D11DepthStencilView* pDepthStencilView);         //

// DEFINE_MEMBER_HOOK(ID3D11DeviceContext, DispatchIndirect, void, //
//                   ID3D11Buffer *pBufferForArgs,                //
//                   UINT AlignedByteOffsetForArgs);              //
//
// DEFINE_MEMBER_HOOK(ID3D11DeviceContext, Dispatch, void, //
//                   UINT ThreadGroupCountX,              //
//                   UINT ThreadGroupCountY,              //
//                   UINT ThreadGroupCountZ);             //
//
// DEFINE_MEMBER_HOOK(ID3D11DeviceContext, ClearState, void);

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

