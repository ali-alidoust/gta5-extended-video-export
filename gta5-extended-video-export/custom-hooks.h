#pragma once

#include <Windows.h>

#include "..\PolyHook\PolyHook\PolyHook.h"
#include "..\PolyHook\Capstone\include\x86.h"

#pragma comment(lib, "..\\PolyHook\\Capstone\\msvc\\x64\\Release\\capstone.lib")

namespace {
	template <class CLASS_TYPE, class FUNC_TYPE> int hookVirtualFunction(CLASS_TYPE *pInstance, int vFuncIndex, LPVOID hookFunc, FUNC_TYPE *originalFunc, std::shared_ptr<PLH::VFuncDetour> VFuncDetour_Ex) {
		if (VFuncDetour_Ex->GetOriginal<FUNC_TYPE>() != NULL) {
			*originalFunc = VFuncDetour_Ex->GetOriginal<FUNC_TYPE>();
			return 0;
		}
		VFuncDetour_Ex->SetupHook(*(BYTE***)pInstance, vFuncIndex, (BYTE*)hookFunc);
		VFuncDetour_Ex->Hook();
		*originalFunc = VFuncDetour_Ex->GetOriginal<FUNC_TYPE>();
		return 0;
	}
	
	template <class FUNC_TYPE>
	int hookNamedFunction(LPCSTR dllname, LPCSTR funcName, LPVOID hookFunc, FUNC_TYPE *originalFunc, std::shared_ptr<PLH::IATHook> IATHook_ex) {
		/*if (IATHook_ex->GetOriginal<FUNC_TYPE>() != NULL) {
			*originalFunc = IATHook_ex->GetOriginal<FUNC_TYPE>();
			return 0;
		}*/
		IATHook_ex->SetupHook(dllname, funcName, (BYTE*)hookFunc);
		IATHook_ex->Hook();
		*originalFunc = IATHook_ex->GetOriginal<FUNC_TYPE>();
		return 0;
	}
}