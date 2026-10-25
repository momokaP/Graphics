// Standard Application Framework Extensions
// AFX는 MFC가 개발되던 초기의 내부 이름
// 미리 컴파일된 헤더(Precompiled Header, PCH) 용 파일

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

// windows.h가 잘 안 쓰이는 헤더들을 빼고 가볍게 include되도록 하는 매크로
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include<d3d12.h>
#include<dxgi1_6.h>
#include<D3Dcompiler.h>
#include<DirectXMath.h>
#include"include/d3dx12/d3dx12.h"

#include<string>
#include<wrl.h> // Windows Runtime C++ Template Library, ComPtr
#include<shellapi.h>