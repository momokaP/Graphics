#include "stdafx.h"
#include "MyEngine.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	MyEngine sample(1280, 720, L"MyEngine");
	return Win32Application::Run(&sample, hInstance, nCmdShow);
}

int main()
{
	return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWDEFAULT);
}