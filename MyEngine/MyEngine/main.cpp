#include "stdafx.h"

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
