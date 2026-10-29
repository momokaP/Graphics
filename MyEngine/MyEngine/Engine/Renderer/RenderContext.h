#pragma once
#include <d3d12.h>

class LinearUploadAllocator;
class DescriptorHeap;
class Texture;
class GpuResource;
struct RenderScene;
namespace DirectX { class ResourceUploadBatch; }

// Init/Resize 때 패스에 주는 것: 리소스를 만들 도구
struct RenderInitContext
{
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;
    DescriptorHeap* rtvHeap = nullptr;
    DescriptorHeap* dsvHeap = nullptr;
    DescriptorHeap* srvHeap = nullptr;
    DXGI_FORMAT          backBufferFormat = DXGI_FORMAT_UNKNOWN;
    UINT                 frameCount = 0;
    DirectX::ResourceUploadBatch* upload = nullptr;    // Init 동안에만 유효 (Resize 때는 nullptr)
};

// 프레임마다 조율 쪽이 준비해서 패스들이 공유하는 렌더 타깃
struct RenderTargets
{
    Texture* sceneColor = nullptr;   // HDR
    Texture* depth = nullptr;
    const Texture* bloom = nullptr;   // BloomPass 실행 뒤에 채워짐
    GpuResource* backBuffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv{};
    UINT width = 0, height = 0;            // 창 크기
};

// Execute 때 패스에 주는 것: 기록할 커맨드 리스트와 이번 프레임의 데이터
struct RenderContext
{
    ID3D12GraphicsCommandList* cmd = nullptr;
    LinearUploadAllocator* upload = nullptr;
    const RenderScene* scene = nullptr;
    RenderTargets* targets = nullptr;
};