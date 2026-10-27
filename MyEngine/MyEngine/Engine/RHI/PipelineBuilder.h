#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>

class PipelineBuilder
{
public:
    explicit PipelineBuilder(ID3D12RootSignature* rootSignature);   // 공통 기본값 세팅

    // ⚠ 바이트코드는 포인터만 저장 → code 벡터는 Build()가 끝날 때까지 살아 있어야 함
    PipelineBuilder& VS(const std::vector<uint8_t>& code);
    PipelineBuilder& PS(const std::vector<uint8_t>& code);

    PipelineBuilder& InputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, UINT count);
    template<size_t N>
    PipelineBuilder& InputLayout(const D3D12_INPUT_ELEMENT_DESC(&elements)[N]) { return InputLayout(elements, UINT(N)); }

    PipelineBuilder& RenderTarget(DXGI_FORMAT format);    // 여러 번 호출하면 MRT (G-Buffer 등)
    PipelineBuilder& DepthFormat(DXGI_FORMAT format);     // 기본값 UNKNOWN = 깊이 버퍼 없음
    PipelineBuilder& Cull(D3D12_CULL_MODE mode);
    PipelineBuilder& DepthTest(bool enable, D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS);
    PipelineBuilder& DepthWrite(bool enable);
    PipelineBuilder& AlphaBlend();                        // 0번 RT에 일반 알파 블렌딩
    PipelineBuilder& Topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);

    Microsoft::WRL::ComPtr<ID3D12PipelineState> Build(ID3D12Device* device, const wchar_t* name);

private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_desc = {};
};