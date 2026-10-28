#pragma once

#include "DXSampleHelper.h"
#include "Win32Application.h"

class DXSample
{
public:
	DXSample(UINT width, UINT height, std::wstring name);
	virtual ~DXSample();

	virtual void OnInit() = 0;
	virtual void OnUpdate(float dt) = 0;
	virtual void OnRender() = 0;
	virtual void OnDestroy() = 0;
	virtual void UpdateGUI() = 0;
	virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;

	// Samples override the event handlers to handle specific messages.
	virtual void OnKeyDown(UINT8 /*key*/) {}
	virtual void OnKeyUp(UINT8 /*ket*/) {}

	// Accessors.
	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }
	const WCHAR* GetTitle() const { return m_title.c_str(); }

	void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
	std::wstring GetAssetFullPath(LPCWSTR assetName);

	void SetCustomWindowText(LPCWSTR text);

	void UpdateForSizeChange(UINT clientWidth, UINT clientHeight)
	{
		m_width = clientWidth;
		m_height = clientHeight;
		m_aspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
	}

	UINT m_width;
	UINT m_height;

	float m_aspectRatio;

	bool m_useWarpDevice;

private:
	std::wstring m_assetsPath;

	std::wstring m_title;
};
