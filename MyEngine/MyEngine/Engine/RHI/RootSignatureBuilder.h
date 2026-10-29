#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <deque>

#include "include/d3dx12/d3dx12.h"

class RootSignatureBuilder
{
public:
    RootSignatureBuilder() = default;
    RootSignatureBuilder(const RootSignatureBuilder&) = delete;            // m_params가 m_ranges 주소를 들고 있음
    RootSignatureBuilder& operator=(const RootSignatureBuilder&) = delete;

    // 디스크립터 테이블 (범위 1개짜리)
    RootSignatureBuilder& Table(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDescriptors, UINT baseRegister,
        D3D12_SHADER_VISIBILITY visibility,
        D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        UINT space = 0);
    // 루트 CBV: 디스크립터 없이 GPU 주소를 직접 바인딩 (1-6, 2단계에서 사용)
    RootSignatureBuilder& CBV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility, UINT space = 0);
    // 루트 상수: 32비트 값 N개를 루트 시그니처에 직접 (후처리 파라미터)
    RootSignatureBuilder& Constants(UINT num32BitValues, UINT shaderRegister,
        D3D12_SHADER_VISIBILITY visibility, UINT space = 0);
    RootSignatureBuilder& StaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);
    RootSignatureBuilder& Flags(D3D12_ROOT_SIGNATURE_FLAGS flags);

    UINT ParamCount() const { return UINT(m_params.size()); }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> Build(ID3D12Device* device, const wchar_t* name);

private:
    std::vector<CD3DX12_ROOT_PARAMETER1>    m_params;
    std::deque<CD3DX12_DESCRIPTOR_RANGE1>   m_ranges;     // ⚠ vector가 아니라 deque (아래 설명)
    std::vector<D3D12_STATIC_SAMPLER_DESC>  m_samplers;
    D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
};

// 자주 쓰는 정적 샘플러
namespace StaticSamplers
{
    D3D12_STATIC_SAMPLER_DESC PointBorder(UINT reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
    D3D12_STATIC_SAMPLER_DESC LinearClamp(UINT reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
    D3D12_STATIC_SAMPLER_DESC LinearWrap(UINT reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
    D3D12_STATIC_SAMPLER_DESC PointClamp(UINT reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
    D3D12_STATIC_SAMPLER_DESC AnisoWrap(UINT reg, UINT maxAnisotropy = 16, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
}