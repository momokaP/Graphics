#pragma once
#include <wrl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <d3dcommon.h> // D3D_SHADER_MODEL
#include <map>

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

struct ShaderResult
{
	bool ok = false;
	std::vector<uint8_t> bytecode;
	std::string messages;  // 에러 + 경고 (성공해도 경고가 있을 수 있음)
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
	ShaderResult Compile(const ShaderDesc& desc); // 실패해도 예외 없음
	std::vector<uint8_t> CompileOrThrow(const ShaderDesc& desc); // Compile을 감싸 실패 시 예외
	
	const std::filesystem::path& SourceDir() const { return m_sourceDir; }
	
	// ── 에러 목록: 파일 이름 → 마지막 에러 메시지 (성공하면 그 파일의 항목이 지워짐) ──
	const std::map<std::string, std::string>& Errors() const { return m_errors; }
	void ReportError(const std::string& key, const std::string& message) { m_errors[key] = message; }
	std::string ErrorSummary() const; // 메시지 박스용으로 전부 이어 붙임

private:
	Microsoft::WRL::ComPtr<IDxcUtils> m_utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> m_compiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> m_includeHandler;
	std::filesystem::path m_sourceDir;
	std::map<std::string, std::string> m_errors;
};