#include "SceneCommon.hlsli"

// 색 대신 엔티티 번호를 출력. R32_UINT 렌더 타깃이라 반환형도 uint
uint main(PSInput input) : SV_TARGET
{
    return g_debug.objectId;
}