#pragma once

/// Filename: MFUtility.h
///
/// Description:
/// This header file contains common macros and functions that are used in the Media Foundation
/// sample applications.
///
/// History:
/// 07 Mar 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

#include <codecapi.h>
#include <cstdio>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>
#include <sstream>

#define DEBUGOUT( s )            \
{                             \
   std::wostringstream os_;    \
   os_ << s << std::endl;                   \
   OutputDebugStringW( os_.str().c_str() );  \
}

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

#define CHECKHR_GOTO(x, y) if(FAILED(x)) goto y

#define INTERNAL_GUID_TO_STRING( _Attribute, _skip ) \
if (Attr == _Attribute) \
{ \
	pAttrStr = #_Attribute; \
	C_ASSERT((sizeof(#_Attribute) / sizeof(#_Attribute[0])) > _skip); \
	pAttrStr += _skip; \
	goto done; \
} \

template <class T> inline void SafeRelease(T*& pT)
{
	if (pT != NULL)
	{
		pT->Release();
		pT = NULL;
	}
}

void GUIDToString(const GUID& guidId, LPSTR buffer, int length) {
	sprintf_s(buffer, length, "%08lX-%04hX-%04hX-%02hX%02hX-%02hX%02hX%02hX%02hX%02hX%02hX",
		guidId.Data1, guidId.Data2, guidId.Data3,
		guidId.Data4[0], guidId.Data4[1], guidId.Data4[2], guidId.Data4[3],
		guidId.Data4[4], guidId.Data4[5], guidId.Data4[6], guidId.Data4[7]);
}

LPCSTR STRING_FROM_GUID(GUID Attr)
{
	LPCSTR pAttrStr = NULL;

	// Generics
	INTERNAL_GUID_TO_STRING(MF_MT_MAJOR_TYPE, 6);                     // MAJOR_TYPE
	INTERNAL_GUID_TO_STRING(MF_MT_SUBTYPE, 6);                        // SUBTYPE
	INTERNAL_GUID_TO_STRING(MF_MT_ALL_SAMPLES_INDEPENDENT, 6);        // ALL_SAMPLES_INDEPENDENT   
	INTERNAL_GUID_TO_STRING(MF_MT_FIXED_SIZE_SAMPLES, 6);             // FIXED_SIZE_SAMPLES
	INTERNAL_GUID_TO_STRING(MF_MT_COMPRESSED, 6);                     // COMPRESSED
	INTERNAL_GUID_TO_STRING(MF_MT_SAMPLE_SIZE, 6);                    // SAMPLE_SIZE
	INTERNAL_GUID_TO_STRING(MF_MT_USER_DATA, 6);                      // MF_MT_USER_DATA

																	  // Audio
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_NUM_CHANNELS, 12);            // NUM_CHANNELS
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_SAMPLES_PER_SECOND, 12);      // SAMPLES_PER_SECOND
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12);    // AVG_BYTES_PER_SECOND
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_BLOCK_ALIGNMENT, 12);         // BLOCK_ALIGNMENT
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_BITS_PER_SAMPLE, 12);         // BITS_PER_SAMPLE
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, 12);   // VALID_BITS_PER_SAMPLE
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_SAMPLES_PER_BLOCK, 12);       // SAMPLES_PER_BLOCK
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_CHANNEL_MASK, 12);            // CHANNEL_MASK
	INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 12);     // PREFER_WAVEFORMATEX

																	  // Video
	INTERNAL_GUID_TO_STRING(MF_MT_FRAME_SIZE, 6);                     // FRAME_SIZE
	INTERNAL_GUID_TO_STRING(MF_MT_FRAME_RATE, 6);                     // FRAME_RATE
	INTERNAL_GUID_TO_STRING(MF_MT_PIXEL_ASPECT_RATIO, 6);             // PIXEL_ASPECT_RATIO
	INTERNAL_GUID_TO_STRING(MF_MT_INTERLACE_MODE, 6);                 // INTERLACE_MODE
	INTERNAL_GUID_TO_STRING(MF_MT_AVG_BITRATE, 6);                    // AVG_BITRATE
	INTERNAL_GUID_TO_STRING(MF_MT_DEFAULT_STRIDE, 6);				  // STRIDE
	INTERNAL_GUID_TO_STRING(MF_MT_AVG_BIT_ERROR_RATE, 6);
	INTERNAL_GUID_TO_STRING(MF_MT_GEOMETRIC_APERTURE, 6);
	INTERNAL_GUID_TO_STRING(MF_MT_MINIMUM_DISPLAY_APERTURE, 6);
	INTERNAL_GUID_TO_STRING(MF_MT_PAN_SCAN_APERTURE, 6);
	INTERNAL_GUID_TO_STRING(MF_MT_VIDEO_NOMINAL_RANGE, 6);

	// Major type values
	INTERNAL_GUID_TO_STRING(MFMediaType_Default, 12);                 // Default
	INTERNAL_GUID_TO_STRING(MFMediaType_Audio, 12);                   // Audio
	INTERNAL_GUID_TO_STRING(MFMediaType_Video, 12);                   // Video
	INTERNAL_GUID_TO_STRING(MFMediaType_Script, 12);                  // Script
	INTERNAL_GUID_TO_STRING(MFMediaType_Image, 12);                   // Image
	INTERNAL_GUID_TO_STRING(MFMediaType_HTML, 12);                    // HTML
	INTERNAL_GUID_TO_STRING(MFMediaType_Binary, 12);                  // Binary
	INTERNAL_GUID_TO_STRING(MFMediaType_SAMI, 12);                    // SAMI
	INTERNAL_GUID_TO_STRING(MFMediaType_Protected, 12);               // Protected

																	  // Minor video type values
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Base, 14);                  // Base
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MP43, 14);                  // MP43
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV1, 14);                  // WMV1
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV2, 14);                  // WMV2
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV3, 14);                  // WMV3
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MPG1, 14);                  // MPG1
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MPG2, 14);                  // MPG2
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB24, 14);				  // RGB24
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB32, 14);				  // RGB32
	INTERNAL_GUID_TO_STRING(MFVideoFormat_H264, 14);				  // H264


																	  // Minor audio type values
	INTERNAL_GUID_TO_STRING(MFAudioFormat_Base, 14);                  // Base
	INTERNAL_GUID_TO_STRING(MFAudioFormat_PCM, 14);                   // PCM
	INTERNAL_GUID_TO_STRING(MFAudioFormat_DTS, 14);                   // DTS
	INTERNAL_GUID_TO_STRING(MFAudioFormat_Dolby_AC3_SPDIF, 14);       // Dolby_AC3_SPDIF
	INTERNAL_GUID_TO_STRING(MFAudioFormat_Float, 14);                 // IEEEFloat
	INTERNAL_GUID_TO_STRING(MFAudioFormat_WMAudioV8, 14);             // WMAudioV8
	INTERNAL_GUID_TO_STRING(MFAudioFormat_WMAudioV9, 14);             // WMAudioV9
	INTERNAL_GUID_TO_STRING(MFAudioFormat_WMAudio_Lossless, 14);      // WMAudio_Lossless
	INTERNAL_GUID_TO_STRING(MFAudioFormat_WMASPDIF, 14);              // WMASPDIF
	INTERNAL_GUID_TO_STRING(MFAudioFormat_MP3, 14);                   // MP3
	INTERNAL_GUID_TO_STRING(MFAudioFormat_MPEG, 14);                  // MPEG

																	  // Media sub types
																	  //INTERNAL_GUID_TO_STRING(WMMEDIASUBTYPE_I420, 15);                  // I420
																	  //INTERNAL_GUID_TO_STRING(WMMEDIASUBTYPE_WVC1, 0);
																	  //INTERNAL_GUID_TO_STRING(WMMEDIASUBTYPE_WMAudioV8, 0);

																	  // MP4 Media Subtypes.
	INTERNAL_GUID_TO_STRING(MF_MT_MPEG4_SAMPLE_DESCRIPTION, 6);
	INTERNAL_GUID_TO_STRING(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY, 6);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncAdaptiveMode, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncCommonBufferSize, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncCommonMaxBitRate, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncCommonMeanBitRate, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncCommonQuality, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncCommonQualityVsSpeed, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncCommonRateControlMode, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncH264CABACEnable, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncH264SPSID, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncMPVDefaultBPictureCount, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncMPVGOPSize, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncNumWorkerThreads, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncVideoContentType, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncVideoEncodeQP, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncVideoForceKeyFrame, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVEncVideoMinQP, 9);
	INTERNAL_GUID_TO_STRING(CODECAPI_AVLowLatencyMode, 9);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB8, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB555, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB565, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB24, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB32, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_ARGB32, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_AI44, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_AYUV, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_I420, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_IYUV, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_NV11, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_NV12, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_UYVY, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y41P, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y41T, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y42T, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_YUY2, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_YVU9, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_YV12, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_YVYU, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_P010, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_P016, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_P210, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_P216, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_v210, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_v216, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_v410, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y210, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y216, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y410, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_Y416, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DV25, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DV50, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DVC, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DVH1, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DVHD, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DVSD, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_DVSL, 0);
	//INTERNAL_GUID_TO_STRING(MFVideoFormat_H263, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_H264, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_H264_ES, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_HEVC, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_HEVC_ES, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_M4S2, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MJPG, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MP43, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MP4S, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MP4V, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MPEG2, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MPG1, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MSS1, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_MSS2, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV1, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV2, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV3, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_WVC1, 0);
	INTERNAL_GUID_TO_STRING(MFVideoFormat_420O, 0);
	

done:
	return pAttrStr;
}

std::string GetMediaTypeDescription(IMFMediaType * pMediaType)
{
	HRESULT hr = S_OK;
	GUID MajorType;
	UINT32 cAttrCount;
	LPCSTR pszGuidStr;
	std::stringstream description;
	WCHAR TempBuf[200];

	if (pMediaType == NULL)
	{
		description << "<NULL>";
		goto done;
	}

	hr = pMediaType->GetMajorType(&MajorType);
	CHECKHR_GOTO(hr, done);

	pszGuidStr = STRING_FROM_GUID(MajorType);
	if (pszGuidStr != NULL)
	{
		description << pszGuidStr;
		description << ": ";
	}
	else
	{
		description << "Other: ";
	}

	hr = pMediaType->GetCount(&cAttrCount);
	CHECKHR_GOTO(hr, done);

	for (UINT32 i = 0; i < cAttrCount; i++)
	{
		GUID guidId;
		MF_ATTRIBUTE_TYPE attrType;

		hr = pMediaType->GetItemByIndex(i, &guidId, NULL);
		CHECKHR_GOTO(hr, done);

		hr = pMediaType->GetItemType(guidId, &attrType);
		CHECKHR_GOTO(hr, done);

		pszGuidStr = STRING_FROM_GUID(guidId);
		if (pszGuidStr != NULL)
		{
			description << pszGuidStr;
		}
		else
		{
			char buffer[64];
			GUIDToString(guidId, buffer, 64);
			description << buffer;
		}

		description << "=";

		switch (attrType)
		{
		case MF_ATTRIBUTE_UINT32:
		{
			UINT32 Val;
			hr = pMediaType->GetUINT32(guidId, &Val);
			CHECKHR_GOTO(hr, done);

			description << Val;
			break;
		}
		case MF_ATTRIBUTE_UINT64:
		{
			UINT64 Val;
			hr = pMediaType->GetUINT64(guidId, &Val);
			CHECKHR_GOTO(hr, done);

			if (guidId == MF_MT_FRAME_SIZE)
			{
				//tempStr.Format("W %u, H: %u", HI32(Val), LO32(Val));
				description << "W:" << HI32(Val) << " H:" << LO32(Val);
			}
			else if ((guidId == MF_MT_FRAME_RATE) || (guidId == MF_MT_PIXEL_ASPECT_RATIO))
			{
				//tempStr.Format("W %u, H: %u", HI32(Val), LO32(Val));
				description << "W:" << HI32(Val) << " H:" << LO32(Val);
			}
			else
			{
				//tempStr.Format("%ld", Val);
				description << Val;
			}

			//description += tempStr;

			break;
		}
		case MF_ATTRIBUTE_DOUBLE:
		{
			DOUBLE Val;
			hr = pMediaType->GetDouble(guidId, &Val);
			CHECKHR_GOTO(hr, done);

			//tempStr.Format("%f", Val);
			description << Val;
			break;
		}
		case MF_ATTRIBUTE_GUID:
		{
			GUID Val;
			const char * pValStr;

			hr = pMediaType->GetGUID(guidId, &Val);
			CHECKHR_GOTO(hr, done);

			pValStr = STRING_FROM_GUID(Val);
			if (pValStr != NULL)
			{
				description << pValStr;
			}
			else
			{
				char buffer[64];
				GUIDToString(guidId, buffer, 64);
				description << buffer;
			}

			break;
		}
		case MF_ATTRIBUTE_STRING:
		{
			hr = pMediaType->GetString(guidId, TempBuf, sizeof(TempBuf) / sizeof(TempBuf[0]), NULL);
			if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
			{
				description << "<Too Long>";
				break;
			}
			CHECKHR_GOTO(hr, done);

			//description += CW2A(TempBuf);
			description << "XXX";// TempBuf;

			break;
		}
		case MF_ATTRIBUTE_BLOB:
		{
			description << "<BLOB>";
			break;
		}
		case MF_ATTRIBUTE_IUNKNOWN:
		{
			description << "<UNK>";
			break;
		}
		//default:
		//assert(0);
		}

		description << "; ";
	}

	//assert(m_szResp.GetLength() >= 2);
	//m_szResp.Left(m_szResp.GetLength() - 2);

done:
	std::string s = description.str();
	
	return s;
}