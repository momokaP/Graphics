#pragma once

#include "DXSampleHelper.h"
#include "Win32Application.h"

struct VertexBuffer {
	ComPtr<ID3D12Resource>   resource;   // 실제 GPU 메모리 (수명 관리)
	D3D12_VERTEX_BUFFER_VIEW view;       // IASetVertexBuffers에 넘길 것
};

struct IndexBuffer {
	ComPtr<ID3D12Resource>  resource;
	D3D12_INDEX_BUFFER_VIEW view;
	UINT                    count;       // DrawIndexedInstanced에 필요
};

struct ConstantBuffer
{
	ComPtr<ID3D12Resource> resource;   // GPU 메모리 (수명 관리)
	UINT8* mapped = nullptr;           // CPU에서 쓸 주소 (계속 매핑된 상태)
	UINT   size = 0;                   // 256 정렬된 실제 버퍼 크기
};

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

	void GetHardwareAdapter(
		_In_ IDXGIFactory1* pFactory,
		_Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
		bool requestHighPerformanceAdapter = false);

	void SetCustomWindowText(LPCWSTR text);

	ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, UINT64 sizeInBytes);
	void CopyToBuffer(ID3D12Resource* buffer, const void* data, size_t sizeInBytes);  // Map-memcpy-Unmap

	VertexBuffer CreateVertexBufferRaw(ID3D12Device* device, const void* data, UINT vertexCount, UINT stride);
	IndexBuffer  CreateIndexBufferRaw(ID3D12Device* device, const void* data, UINT indexCount, DXGI_FORMAT format);
	ConstantBuffer CreateConstantBufferRaw(ID3D12Device* device, const void* initData, UINT dataSize, D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle);

	template<typename TVertex>
	VertexBuffer CreateVertexBuffer(ID3D12Device* device, const std::vector<TVertex>& vertices)
	{
		return CreateVertexBufferRaw(device, vertices.data(),
			static_cast<UINT>(vertices.size()), sizeof(TVertex));
	}

	template<typename TIndex>
	IndexBuffer CreateIndexBuffer(ID3D12Device* device, const std::vector<TIndex>& indices)
	{
		static_assert(sizeof(TIndex) == 2 || sizeof(TIndex) == 4, "Index must be 16 or 32 bit");
		constexpr DXGI_FORMAT format = (sizeof(TIndex) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		return CreateIndexBufferRaw(device, indices.data(),
			static_cast<UINT>(indices.size()), format);
	}

	template<typename TConst>
	ConstantBuffer CreateConstantBuffer(ID3D12Device* device, const TConst& initData,
		D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle)
	{
		return CreateConstantBufferRaw(device, &initData, sizeof(TConst), cbvHandle);
	}

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
