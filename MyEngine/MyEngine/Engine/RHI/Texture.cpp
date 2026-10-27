#include "stdafx.h"
#include "RHI/Texture.h"
#include "DXSampleHelper.h"
#include <directxtk12/ResourceUploadBatch.h>
#include <directxtk12/WICTextureLoader.h>
#include <directxtk12/DDSTextureLoader.h>

void Texture::CreateRenderTarget(ID3D12Device* device, DescriptorHeap& rtvHeap, DescriptorHeap& srvHeap,
    UINT width, UINT height, DXGI_FORMAT format,
    const float clearColor[4], const wchar_t* name)
{
    memcpy(m_clearColor, clearColor, sizeof(m_clearColor));

    D3D12_CLEAR_VALUE clear = {};          // 최적화 클리어 값: Clear 때 같은 색을 쓰면 빠름
    clear.Format = format;
    memcpy(clear.Color, clearColor, sizeof(clear.Color));

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    m_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;   // 평소 상태는 SRV, 그릴 때만 RT로
    ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, m_state,
        &clear, IID_PPV_ARGS(m_resource.ReleaseAndGetAddressOf())));
    SetName(name);
    m_isCube = false;

    if (!rtv.IsValid()) rtv = rtvHeap.Allocate();
    if (!srv.IsValid()) srv = srvHeap.Allocate();
    device->CreateRenderTargetView(m_resource.Get(), nullptr, rtv.cpu);
    WriteSrv(device);
}

void Texture::CreateDepth(ID3D12Device* device, DescriptorHeap& dsvHeap,
    UINT width, UINT height, DXGI_FORMAT format, const wchar_t* name)
{
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = format;
    clear.DepthStencil.Depth = 1.0f;

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    m_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, m_state,
        &clear, IID_PPV_ARGS(m_resource.ReleaseAndGetAddressOf())));
    SetName(name);

    if (!dsv.IsValid()) dsv = dsvHeap.Allocate();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, dsv.cpu);
}

bool Texture::LoadWIC(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
    const std::filesystem::path& path, bool srgb)
{
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(device, upload, path.c_str(), 0,
        D3D12_RESOURCE_FLAG_NONE,
        srgb ? DirectX::WIC_LOADER_FORCE_SRGB : DirectX::WIC_LOADER_DEFAULT,
        m_resource.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;            // 모델 텍스처는 없을 수도 있으니 예외 대신 false

    // ResourceUploadBatch는 복사가 끝나면 텍스처를 PIXEL_SHADER_RESOURCE로 전환해 둠
    m_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_isCube = false;
    SetName(path.filename().c_str());
    return true;
}

void Texture::LoadDDS(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
    const std::filesystem::path& path)
{
    ThrowIfFailed(DirectX::CreateDDSTextureFromFileEx(device, upload, path.c_str(), 0,
        D3D12_RESOURCE_FLAG_NONE, DirectX::DDS_LOADER_DEFAULT,
        m_resource.ReleaseAndGetAddressOf(), nullptr, &m_isCube));
    m_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    SetName(path.filename().c_str());
}

void Texture::CreateSrv(ID3D12Device* device, DescriptorHeap& srvHeap)
{
    if (!srv.IsValid()) srv = srvHeap.Allocate();
    WriteSrv(device);
}

void Texture::WriteSrv(ID3D12Device* device)
{
    const D3D12_RESOURCE_DESC d = m_resource->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Format = d.Format;
    if (m_isCube)
    {
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        desc.TextureCube.MipLevels = d.MipLevels;
    }
    else
    {
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = d.MipLevels;
    }
    device->CreateShaderResourceView(m_resource.Get(), &desc, srv.cpu);
}