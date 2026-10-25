#pragma once

#include <filesystem>
#include <string>

class Paths
{
public:
    static void Init(const std::filesystem::path& assetOverride = {});

    static const std::filesystem::path& ExeDir() { return s_exeDir; }
    static const std::filesystem::path& AssetDir() { return s_assetDir; }

    static std::filesystem::path Asset(const std::filesystem::path& rel) { return s_assetDir / rel; }
    static std::filesystem::path Shader(const std::filesystem::path& name) { return s_exeDir / name; }

    static std::string ToUtf8(const std::filesystem::path& p)
    {
        std::u8string s = p.u8string();                    // C++20에선 std::u8string 반환
        return std::string(s.begin(), s.end());
    }

    static std::filesystem::path FromUtf8(const std::string& s)
    {
        return std::filesystem::path(std::u8string(s.begin(), s.end()));   // UTF-8로 해석하게 강제
    }

private:
    static inline std::filesystem::path s_exeDir;
    static inline std::filesystem::path s_assetDir;
};