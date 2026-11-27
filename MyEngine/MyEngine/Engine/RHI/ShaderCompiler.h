#pragma once
#include <wrl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <d3dcommon.h> // D3D_SHADER_MODEL

// 셰이더 모델은 이곳에서만 정한다. 두 줄은 항상 같은 버전이어야 함
#define ENGINE_SM_SUFFIX L"6_6"
inline constexpr D3D_SHADER_MODEL kRequiredShaderModel = D3D_SHADER_MODEL_6_6;

struct IDxcUtils;
struct IDxcCompiler3;
struct IDxcIncludeHandler;

// 셰이더 타깃은 이곳에서만 정한다
namespace ShaderTarget
{
	inline constexpr const wchar_t* VS = L"vs_" ENGINE_SM_SUFFIX;
	inline constexpr const wchar_t* PS = L"ps_" ENGINE_SM_SUFFIX;
	inline constexpr const wchar_t* CS = L"cs_" ENGINE_SM_SUFFIX;
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