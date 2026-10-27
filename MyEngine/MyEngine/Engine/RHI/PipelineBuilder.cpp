#include "stdafx.h"
#include "RHI/PipelineBuilder.h"
#include "DXSampleHelper.h"
#include <cassert>

PipelineBuilder::PipelineBuilder(ID3D12RootSignature* rootSignature)
{
    m_desc.pRootSignature = rootSignature;
    m_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);      // Solid, Cull Back
    m_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);           // 블렌딩 없음
    m_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);   // 깊이 LESS, 쓰기 ON
    m_desc.SampleMask = UINT_MAX;
    m_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    m_desc.SampleDesc.Count = 1;
    m_desc.NumRenderTargets = 0;
    m_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
}

PipelineBuilder& PipelineBuilder::VS(const std::vector<uint8_t>& code) { m_desc.VS = { code.data(), code.size() }; return *this; }
PipelineBuilder& PipelineBuilder::PS(const std::vector<uint8_t>& code) { m_desc.PS = { code.data(), code.size() }; return *this; }

PipelineBuilder& PipelineBuilder::InputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, UINT count)
{
    m_desc.InputLayout = { elements, count };
    return *this;
}

PipelineBuilder& PipelineBuilder::RenderTarget(DXGI_FORMAT format)
{
    assert(m_desc.NumRenderTargets < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
    m_desc.RTVFormats[m_desc.NumRenderTargets++] = format;
    return *this;
}

PipelineBuilder& PipelineBuilder::DepthFormat(DXGI_FORMAT format) { m_desc.DSVFormat = format; return *this; }
PipelineBuilder& PipelineBuilder::Cull(D3D12_CULL_MODE mode) { m_desc.RasterizerState.CullMode = mode; return *this; }

PipelineBuilder& PipelineBuilder::DepthTest(bool enable, D3D12_COMPARISON_FUNC func)
{
    m_desc.DepthStencilState.DepthEnable = enable ? TRUE : FALSE;
    m_desc.DepthStencilState.DepthFunc = func;
    return *this;
}

PipelineBuilder& PipelineBuilder::DepthWrite(bool enable)
{
    m_desc.DepthStencilState.DepthWriteMask = enable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    return *this;
}

PipelineBuilder& PipelineBuilder::AlphaBlend()
{
    auto& rt = m_desc.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;  rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;  rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    return *this;
}

PipelineBuilder& PipelineBuilder::Topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) { m_desc.PrimitiveTopologyType = type; return *this; }

Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineBuilder::Build(ID3D12Device* device, const wchar_t* name)
{
    // 깊이 버퍼가 없는데 깊이 테스트가 켜져 있으면 디버그 레이어 에러 → 자동으로 끔
    if (m_desc.DSVFormat == DXGI_FORMAT_UNKNOWN)
    {
        m_desc.DepthStencilState.DepthEnable = FALSE;
        m_desc.DepthStencilState.StencilEnable = FALSE;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&m_desc, IID_PPV_ARGS(&pso)));
    if (name) pso->SetName(name);    // PIX와 디버그 메시지에 "Forward PSO"처럼 표시
    return pso;
}