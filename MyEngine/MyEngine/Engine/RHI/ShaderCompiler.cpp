#include "stdafx.h"
#include "RHI/ShaderCompiler.h"

#include <dxcapi.h>
#include <stdexcept>
#include "DXSampleHelper.h"

#pragma comment(lib, "dxcompiler.lib")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

ShaderCompiler::ShaderCompiler() = default;
ShaderCompiler::~ShaderCompiler() = default;

void ShaderCompiler::Init(const fs::path& sourceDir)
{
	m_sourceDir = sourceDir;
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler)));
	ThrowIfFailed(m_utils->CreateDefaultIncludeHandler(&m_includeHandler)); // #include "X.hlsli" 처리
}

std::vector<uint8_t> ShaderCompiler::CompileOrThrow(const ShaderDesc& desc)
{
	const fs::path fullPath = m_sourceDir / desc.file;

	// ── 1) 소스 읽기 ──
	ComPtr<IDxcBlobEncoding> source;
	if (FAILED(m_utils->LoadFile(fullPath.c_str(), nullptr, &source)))
		throw std::runtime_error("Shader file not found: " + fullPath.string());

	DxcBuffer buffer = {};
	buffer.Ptr = source->GetBufferPointer();
	buffer.Size = source->GetBufferSize();
	buffer.Encoding = DXC_CP_UTF8;

	// ── 2) 인자 (dxc.exe 명령줄과 같은 형식) ──
	const std::wstring includeDir = m_sourceDir.wstring();
	std::vector<LPCWSTR> args = {
		fullPath.c_str(),            // 에러 메시지에 표시할 파일 이름 (첫 인자)
		L"-E", desc.entry.c_str(),
		L"-T", desc.target,
		L"-I", includeDir.c_str(),
		L"-HV", L"2021",             // HLSL 버전 명시 (최신 DXC의 기본값이지만 명시해 둠)
#ifdef _DEBUG
		L"-Od",                      // 최적화 끔: 디버깅 시 변수가 사라지지 않게
		L"-Zi", L"-Qembed_debug",    // 디버그 정보를 DXIL 안에 포함 (4-2에서 PIX 소스 디버깅에 사용)
#else
		L"-O3",
#endif
	};
	for (const std::wstring& d : desc.defines) { args.push_back(L"-D"); args.push_back(d.c_str()); }

	// ── 3) 컴파일 ──
	ComPtr<IDxcResult> result;
	ThrowIfFailed(m_compiler->Compile(&buffer, args.data(), UINT32(args.size()),
		m_includeHandler.Get(), IID_PPV_ARGS(&result))); // 이 HRESULT는 "호출 자체"의 성공 여부

	// ── 4) 에러/경고 메시지 (성공해도 경고가 있을 수 있음) ──
	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
		OutputDebugStringA(errors->GetStringPointer()); // "파일(줄,열): error: ..." → 출력 창에서 더블클릭하면 이동

	HRESULT status = S_OK;
	result->GetStatus(&status);
	if (FAILED(status))
		throw std::runtime_error("Shader compile failed: " + fullPath.string());

	// ── 5) DXIL 꺼내기 ──
	ComPtr<IDxcBlob> object;
	ThrowIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr));
	const auto* p = static_cast<const uint8_t*>(object->GetBufferPointer());
	return std::vector<uint8_t>(p, p + object->GetBufferSize());
}
