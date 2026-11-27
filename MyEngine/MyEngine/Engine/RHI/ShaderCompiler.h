#pragma once
#include <wrl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

struct IDxcUtils;
struct IDxcCompiler3;
struct IDxcIncludeHandler;

// 셰이더 타깃은 이곳에서만 정한다
namespace ShaderTarget
{
	inline constexpr const wchar_t* VS = L"vs_6_0";
	inline constexpr const wchar_t* PS = L"ps_6_0";
	inline constexpr const wchar_t* CS = L"cs_6_0";
}

struct ShaderDesc
{
	std::filesystem::path file;
	const wchar_t* target = nullptr;
	std::wstring entry = L"main";
	std::vector<std::wstring> defines;
};

// DXC 래퍼. DX12 전용 도구라 RHI 계층에 둔다.
class ShaderCompiler
{
public:
	ShaderCompiler();                                    
	~ShaderCompiler();
	ShaderCompiler(const ShaderCompiler&) = delete;
	ShaderCompiler& operator=(const ShaderCompiler&) = delete;

	void Init(const std::filesystem::path& sourceDir);

	std::vector<uint8_t> CompileOrThrow(const ShaderDesc& desc);

	const std::filesystem::path& SourceDir() const { return m_sourceDir; }

private:
	Microsoft::WRL::ComPtr<IDxcUtils> m_utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> m_compiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> m_includeHandler;
	std::filesystem::path m_sourceDir;
};