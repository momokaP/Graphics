#include "stdafx.h"

// ── Agility SDK: exe 옆 D3D12\ 폴더의 D3D12Core.dll을 쓰도록 로더에 알림 ──
//   버전은 NuGet 패키지의 d3d12.h가 정의하는 D3D12_SDK_VERSION과 반드시 같아야 함
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

// ── 실행할 앱 선택 ──
//   1: SandboxApp — 2-5 구조 (Renderer 클래스가 렌더링 전담)
//   0: MyEngine   — 2-4까지의 구조 (비교용, 코드는 그대로 보존)
#define USE_SANDBOX_APP 1

#if USE_SANDBOX_APP
	#include "Sandbox/SandboxApp.h"
	using App = SandboxApp;
	static const wchar_t* kAppTitle = L"MyEngine - SandboxApp (Renderer)";
#else
	#include "MyEngine.h"
	using App = MyEngine;
	static const wchar_t* kAppTitle = L"MyEngine - Legacy";
#endif

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	App app(1280, 720, kAppTitle);
	return Win32Application::Run(&app, hInstance, nCmdShow);
}

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWDEFAULT);
}
