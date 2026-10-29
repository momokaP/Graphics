#pragma once
#include <wrl.h>
#include "Renderer/RenderPass.h"
#include "RHI/Texture.h"

class BloomPass : public RenderPass
{
public:
    const wchar_t* Name() const override { return L"Bloom"; }
    void Init(const RenderInitContext& ctx) override;
    void Resize(const RenderInitContext& ctx, UINT width, UINT height) override;
    void Execute(RenderContext& ctx) override;

    const Texture& Output() const { return m_bloom[0]; }   // 최종 블룸 결과

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_brightPSO, m_blurPSO;
    Texture m_bloom[2];
    UINT m_width = 1, m_height = 1;
};