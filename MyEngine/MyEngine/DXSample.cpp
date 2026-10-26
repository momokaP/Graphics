#include "stdafx.h"
#include "DXSample.h"

using namespace Microsoft::WRL;

DXSample::DXSample(UINT width, UINT height, std::wstring name) :
	m_width(width),
	m_height(height),
	m_title(name),
	m_useWarpDevice(false)
{
	WCHAR assetsPath[512];
	GetAssetsPath(assetsPath, _countof(assetsPath));
	m_assetsPath = assetsPath;

	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

DXSample::~DXSample()
{
}

std::wstring DXSample::GetAssetFullPath(LPCWSTR assetName)
{
	return m_assetsPath + assetName;
}

void DXSample::SetCustomWindowText(LPCWSTR text)
{
	std::wstring windowText = m_title + L": " + text;
	SetWindowText(Win32Application::GetHwnd(), windowText.c_str());
}

ComPtr<ID3D12Resource> DXSample::CreateUploadBuffer(ID3D12Device* device, UINT64 sizeInBytes)
{
	ComPtr<ID3D12Resource> buffer;
	
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&buffer)
	));

	return buffer;
}

void DXSample::CopyToBuffer(ID3D12Resource* buffer, const void* data, size_t sizeInBytes)
{
	UINT8* pDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(buffer->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin)));
	memcpy(pDataBegin, data, sizeInBytes);
	buffer->Unmap(0, nullptr);
}

VertexBuffer DXSample::CreateVertexBufferRaw(ID3D12Device* device, const void* data, UINT vertexCount, UINT stride)
{
	VertexBuffer vertexBuffer;
	const UINT vertexBufferSize = vertexCount * stride;

	vertexBuffer.resource = CreateUploadBuffer(device, vertexBufferSize);
	CopyToBuffer(vertexBuffer.resource.Get(), data, vertexBufferSize);

	vertexBuffer.view.BufferLocation = vertexBuffer.resource->GetGPUVirtualAddress();
	vertexBuffer.view.StrideInBytes = stride;
	vertexBuffer.view.SizeInBytes = vertexBufferSize;

	return vertexBuffer;
}

IndexBuffer DXSample::CreateIndexBufferRaw(ID3D12Device* device, const void* data, UINT indexCount, DXGI_FORMAT format)
{
	assert(format == DXGI_FORMAT_R16_UINT || format == DXGI_FORMAT_R32_UINT);
	const UINT stride = (format == DXGI_FORMAT_R16_UINT) ? 2 : 4;
	const UINT indexBufferSize = indexCount * stride;

	IndexBuffer indexBuffer;
	indexBuffer.count = indexCount;

	indexBuffer.resource = CreateUploadBuffer(device, indexBufferSize);
	CopyToBuffer(indexBuffer.resource.Get(), data, indexBufferSize);
	
	indexBuffer.view.BufferLocation = indexBuffer.resource->GetGPUVirtualAddress();
	indexBuffer.view.Format = format;
	indexBuffer.view.SizeInBytes = indexBufferSize;

	return indexBuffer;
}

ConstantBuffer DXSample::CreateConstantBufferRaw(ID3D12Device* device, const void* initData, UINT dataSize, D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle)
{
	ConstantBuffer constantBuffer;
	
	constantBuffer.size = CalculateConstantBufferByteSize(dataSize);
	constantBuffer.resource = CreateUploadBuffer(device, constantBuffer.size);

	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(constantBuffer.resource->Map(0, &readRange, reinterpret_cast<void**>(&constantBuffer.mapped)));
	memcpy(constantBuffer.mapped, &initData, sizeof(initData));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer.resource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constantBuffer.size;
	device->CreateConstantBufferView(&cbvDesc, cbvHandle);

	return constantBuffer;
}

_Use_decl_annotations_
void DXSample::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
		{
			m_useWarpDevice = true;
			m_title = m_title + L" (WARP)";
		}
	}
}