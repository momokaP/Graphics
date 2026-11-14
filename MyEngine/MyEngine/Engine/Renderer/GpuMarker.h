#pragma once
#include <d3d12.h>

#define USE_PIX            // Release에서도 마커를 쓰려면 명시. 빼면 Debug/Profile에서만 동작
#include <pix3.h>          // WinPixEventRuntime

class GpuMarker
{
public:
    GpuMarker(ID3D12GraphicsCommandList* cmd, const wchar_t* name) : m_cmd(cmd)
    {
        PIXBeginEvent(m_cmd, PIX_COLOR_DEFAULT, L"%s", name);
    }
    ~GpuMarker() { PIXEndEvent(m_cmd); } 

    GpuMarker(const GpuMarker&) = delete;
    GpuMarker& operator=(const GpuMarker&) = delete;

private:
    ID3D12GraphicsCommandList* m_cmd;
};