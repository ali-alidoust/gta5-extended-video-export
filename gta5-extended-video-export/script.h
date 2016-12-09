#pragma once

#include <dxgi.h>
#include <d3d11.h>

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
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
void initialize();
void ScriptMain();
void onPresent(IDXGISwapChain *swapChain);
void finalize();