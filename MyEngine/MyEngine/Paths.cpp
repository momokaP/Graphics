#include "stdafx.h"
#include "Paths.h"
#include <stdexcept>

namespace fs = std::filesystem;

void Paths::Init(const fs::path& assetOverride)
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    s_exeDir = fs::path(buf).parent_path();

    // 셰이더 소스: exe 폴더부터 위로 올라가며 Engine/Shaders 탐색 (개발 중엔 프로젝트 폴더를 찾음)
    for (fs::path dir = s_exeDir; !dir.empty(); dir = dir.parent_path()) {
        if (fs::is_directory(dir / "Engine" / "Shaders")) { s_shaderSourceDir = dir / "Engine" / "Shaders"; break; }
        if (fs::is_directory(dir / "MyEngine" / "Engine" / "Shaders")) { s_shaderSourceDir = dir / "MyEngine" / "Engine" / "Shaders"; break; }
        if (dir == dir.root_path()) break;
    }
    if (s_shaderSourceDir.empty())
        throw std::runtime_error("Engine/Shaders folder not found");

    // 명령줄로 직접 지정 (-assets D:\...)
    if (!assetOverride.empty() && fs::is_directory(assetOverride)) {
        s_assetDir = fs::canonical(assetOverride);
        return;
    }

    // exe 폴더부터 위로 올라가며 Assets/ 탐색
    for (fs::path dir = s_exeDir; !dir.empty(); dir = dir.parent_path()) {
        if (fs::is_directory(dir / "Assets")) {
            s_assetDir = dir / "Assets";
            return;
        }
        if (dir == dir.root_path()) break;
    }

    throw std::runtime_error("Assets folder not found");   // 조용히 실패하지 않고 바로 알 수 있게
}