#pragma once
#include <cstdint>

// 타입이 붙은 핸들: MeshHandle과 TextureHandle은 서로 대입 불가
template<typename Tag>
struct Handle
{
	static constexpr uint32_t kInvalid = UINT32_MAX;
	uint32_t index = kInvalid;

	bool IsValid() const { return index != kInvalid; }
	friend bool operator==(Handle a, Handle b) { return a.index == b.index; }
};

struct MeshTag;
struct TextureTag;
struct MaterialTag;
using MeshHandle = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;
using MaterialHandle = Handle<MaterialTag>;