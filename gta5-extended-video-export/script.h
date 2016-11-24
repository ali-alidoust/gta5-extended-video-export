#pragma once

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

void ScriptMain();
void unhookAllVirtual();