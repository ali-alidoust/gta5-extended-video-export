#pragma once
#include <dxgi.h>
#include <string>
#include "logger.h"

#define RETURN_STR(val, e)                                                                                             \
    {                                                                                                                  \
        if (val == e) {                                                                                                \
            return #e;                                                                                                 \
        }                                                                                                              \
    }

inline static std::string conv_dxgi_format_to_string(int value) {
    RETURN_STR(value, DXGI_FORMAT_UNKNOWN);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32A32_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32A32_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32A32_UINT);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32A32_SINT);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32_UINT);
    RETURN_STR(value, DXGI_FORMAT_R32G32B32_SINT);
    RETURN_STR(value, DXGI_FORMAT_R16G16B16A16_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R16G16B16A16_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R16G16B16A16_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R16G16B16A16_UINT);
    RETURN_STR(value, DXGI_FORMAT_R16G16B16A16_SNORM);
    RETURN_STR(value, DXGI_FORMAT_R16G16B16A16_SINT);
    RETURN_STR(value, DXGI_FORMAT_R32G32_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R32G32_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R32G32_UINT);
    RETURN_STR(value, DXGI_FORMAT_R32G32_SINT);
    RETURN_STR(value, DXGI_FORMAT_R32G8X24_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    RETURN_STR(value, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_X32_TYPELESS_G8X24_UINT);
    RETURN_STR(value, DXGI_FORMAT_R10G10B10A2_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R10G10B10A2_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R10G10B10A2_UINT);
    RETURN_STR(value, DXGI_FORMAT_R11G11B10_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R8G8B8A8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R8G8B8A8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_R8G8B8A8_UINT);
    RETURN_STR(value, DXGI_FORMAT_R8G8B8A8_SNORM);
    RETURN_STR(value, DXGI_FORMAT_R8G8B8A8_SINT);
    RETURN_STR(value, DXGI_FORMAT_R16G16_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R16G16_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R16G16_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R16G16_UINT);
    RETURN_STR(value, DXGI_FORMAT_R16G16_SNORM);
    RETURN_STR(value, DXGI_FORMAT_R16G16_SINT);
    RETURN_STR(value, DXGI_FORMAT_R32_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_D32_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R32_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_R32_UINT);
    RETURN_STR(value, DXGI_FORMAT_R32_SINT);
    RETURN_STR(value, DXGI_FORMAT_R24G8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_D24_UNORM_S8_UINT);
    RETURN_STR(value, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_X24_TYPELESS_G8_UINT);
    RETURN_STR(value, DXGI_FORMAT_R8G8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R8G8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R8G8_UINT);
    RETURN_STR(value, DXGI_FORMAT_R8G8_SNORM);
    RETURN_STR(value, DXGI_FORMAT_R8G8_SINT);
    RETURN_STR(value, DXGI_FORMAT_R16_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R16_FLOAT);
    RETURN_STR(value, DXGI_FORMAT_D16_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R16_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R16_UINT);
    RETURN_STR(value, DXGI_FORMAT_R16_SNORM);
    RETURN_STR(value, DXGI_FORMAT_R16_SINT);
    RETURN_STR(value, DXGI_FORMAT_R8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_R8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R8_UINT);
    RETURN_STR(value, DXGI_FORMAT_R8_SNORM);
    RETURN_STR(value, DXGI_FORMAT_R8_SINT);
    RETURN_STR(value, DXGI_FORMAT_A8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R1_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R9G9B9E5_SHAREDEXP);
    RETURN_STR(value, DXGI_FORMAT_R8G8_B8G8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_G8R8_G8B8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC1_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC1_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC1_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_BC2_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC2_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC2_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_BC3_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC3_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC3_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_BC4_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC4_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC4_SNORM);
    RETURN_STR(value, DXGI_FORMAT_BC5_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC5_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC5_SNORM);
    RETURN_STR(value, DXGI_FORMAT_B5G6R5_UNORM);
    RETURN_STR(value, DXGI_FORMAT_B5G5R5A1_UNORM);
    RETURN_STR(value, DXGI_FORMAT_B8G8R8A8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_B8G8R8X8_UNORM);
    RETURN_STR(value, DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM);
    RETURN_STR(value, DXGI_FORMAT_B8G8R8A8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_B8G8R8X8_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_BC6H_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC6H_UF16);
    RETURN_STR(value, DXGI_FORMAT_BC6H_SF16);
    RETURN_STR(value, DXGI_FORMAT_BC7_TYPELESS);
    RETURN_STR(value, DXGI_FORMAT_BC7_UNORM);
    RETURN_STR(value, DXGI_FORMAT_BC7_UNORM_SRGB);
    RETURN_STR(value, DXGI_FORMAT_AYUV);
    RETURN_STR(value, DXGI_FORMAT_Y410);
    RETURN_STR(value, DXGI_FORMAT_Y416);
    RETURN_STR(value, DXGI_FORMAT_NV12);
    RETURN_STR(value, DXGI_FORMAT_P010);
    RETURN_STR(value, DXGI_FORMAT_P016);
    RETURN_STR(value, DXGI_FORMAT_420_OPAQUE);
    RETURN_STR(value, DXGI_FORMAT_YUY2);
    RETURN_STR(value, DXGI_FORMAT_Y210);
    RETURN_STR(value, DXGI_FORMAT_Y216);
    RETURN_STR(value, DXGI_FORMAT_NV11);
    RETURN_STR(value, DXGI_FORMAT_AI44);
    RETURN_STR(value, DXGI_FORMAT_IA44);
    RETURN_STR(value, DXGI_FORMAT_P8);
    RETURN_STR(value, DXGI_FORMAT_A8P8);
    RETURN_STR(value, DXGI_FORMAT_B4G4R4A4_UNORM);
    RETURN_STR(value, DXGI_FORMAT_FORCE_UINT);

    return "<UNKNOWN FORMAT>";
}

inline void StackDump(size_t size, std::string prefix) {
    uint64_t x = 0xDEADBEEFBAADF00D;
    void** ptr = (void**)&x;
    for (int i = 0; i < size; i++) {
        LOG(LL_TRC, "Stack dump: ", prefix, " ", Logger::hex(i, 4), ": 0x", *(ptr + i));
    }
}

//bool isCurrentRenderTargetView(ID3D11DeviceContext* pCtx, ComPtr<ID3D11RenderTargetView>& pRTV) {
//    if (pRTV == NULL) {
//        return false;
//    }
//    ComPtr<ID3D11RenderTargetView> pCurrentRTV;
//    pCtx->OMGetRenderTargets(1, pCurrentRTV.GetAddressOf(), NULL);
//    return pRTV == pCurrentRTV;
//}

template <class T> void SafeDelete(T*& pVal) {
    delete pVal;
    pVal = NULL;
}

template <class T> void SafeDeleteArray(T*& pVal) {
    delete[] pVal;
    pVal = NULL;
}

inline std::string hexdump(void* ptr, int buflen) {
    unsigned char* buf = (unsigned char*)ptr;
    std::stringstream sstream;
    char temp[4096];
    int i, j;
    for (i = 0; i < buflen; i += 16) {
        sprintf_s(temp, "%06x: ", i);
        sstream << temp;
        for (j = 0; j < 16; j++) {
            if (i + j < buflen) {
                sprintf_s(temp, "%02x ", buf[i + j]);
            } else {
                sprintf_s(temp, "   ");
            }
            sstream << temp;
        }

        sstream << " ";
        for (j = 0; j < 16; j++) {
            if (i + j < buflen) {
                sprintf_s(temp, "%c", isprint(buf[i + j]) ? buf[i + j] : '.');
                sstream << temp;
            }
        }

        sstream << " ";
    }
    return sstream.str();
}

// https://stackoverflow.com/questions/215963/how-do-you-properly-use-widechartomultibyte

// Convert a wide Unicode string to an UTF8 string
inline std::string utf8_encode(const std::wstring& wstr) {
    if (wstr.empty())
        return {};
    const int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
inline std::wstring utf8_decode(const std::string& str) {
    if (str.empty())
        return {};
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string AsiPath(); 

#undef RETURN_STR