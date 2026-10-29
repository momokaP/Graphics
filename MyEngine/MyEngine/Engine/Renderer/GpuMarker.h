#pragma once
#include <d3d12.h>
#include <cwchar>

// 범위 기반 GPU 마커: 생성 시 BeginEvent, 소멸 시 EndEvent → PIX/디버거에서 구간 이름으로 보임
// WinPixEventRuntime 없이 D3D12 기본 API 사용 (metadata 0 = 유니코드 문자열)
class GpuMarker
{
public:
    GpuMarker(ID3D12GraphicsCommandList* cmd, const wchar_t* name) : m_cmd(cmd)
    {
        m_cmd->BeginEvent(0, name, UINT((wcslen(name) + 1) * sizeof(wchar_t)));
    }
    ~GpuMarker() { m_cmd->EndEvent(); }

    GpuMarker(const GpuMarker&) = delete;
    GpuMarker& operator=(const GpuMarker&) = delete;

private:
    ID3D12GraphicsCommandList* m_cmd;
};