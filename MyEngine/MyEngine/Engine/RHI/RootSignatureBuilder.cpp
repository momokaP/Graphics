#include "stdafx.h"
#include "RHI/RootSignatureBuilder.h"
#include "DXSampleHelper.h"

using Microsoft::WRL::ComPtr;

RootSignatureBuilder& RootSignatureBuilder::Table(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDescriptors,
    UINT baseRegister, D3D12_SHADER_VISIBILITY visibility, D3D12_DESCRIPTOR_RANGE_FLAGS flags, UINT space)
{
    m_ranges.emplace_back();
    m_ranges.back().Init(type, numDescriptors, baseRegister, space, flags);

    m_params.emplace_back();
    m_params.back().InitAsDescriptorTable(1, &m_ranges.back(), visibility);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::CBV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility, UINT space)
{
    m_params.emplace_back();
    m_params.back().InitAsConstantBufferView(shaderRegister, space,
        D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::Constants(UINT num32BitValues, UINT shaderRegister,
    D3D12_SHADER_VISIBILITY visibility, UINT space)
{
    m_params.emplace_back();
    m_params.back().InitAsConstants(num32BitValues, shaderRegister, space, visibility);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::StaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    m_samplers.push_back(sampler);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::Flags(D3D12_ROOT_SIGNATURE_FLAGS flags)
{
    m_flags = flags;
    return *this;
}

ComPtr<ID3D12RootSignature> RootSignatureBuilder::Build(ID3D12Device* device, const wchar_t* name)
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature = {};
    feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature, sizeof(feature))))
        feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(UINT(m_params.size()), m_params.data(),
        UINT(m_samplers.size()), m_samplers.data(), m_flags);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, feature.HighestVersion, &blob, &error);
    if (FAILED(hr))
    {
        if (error) OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));   // 실패 이유 출력
        ThrowIfFailed(hr);
    }

    ComPtr<ID3D12RootSignature> rootSig;
    ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSig)));
    if (name) rootSig->SetName(name);
    return rootSig;
}

namespace StaticSamplers
{
    D3D12_STATIC_SAMPLER_DESC PointBorder(UINT reg, D3D12_SHADER_VISIBILITY vis)
    {
        CD3DX12_STATIC_SAMPLER_DESC s(reg, D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
        s.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;   // 기존 설정과 동일하게
        s.ShaderVisibility = vis;
        return s;
    }

    D3D12_STATIC_SAMPLER_DESC LinearClamp(UINT reg, D3D12_SHADER_VISIBILITY vis)
    {
        CD3DX12_STATIC_SAMPLER_DESC s(reg, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        s.ShaderVisibility = vis;
        return s;
    }

    D3D12_STATIC_SAMPLER_DESC LinearWrap(UINT reg, D3D12_SHADER_VISIBILITY vis)
    {
        CD3DX12_STATIC_SAMPLER_DESC s(reg, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        s.ShaderVisibility = vis;
        return s;
    }
}