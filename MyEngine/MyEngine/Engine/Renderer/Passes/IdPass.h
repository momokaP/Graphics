#pragma once
#include <wrl.h>
#include "Renderer/RenderPass.h"

// 엔티티 번호를 R32_UINT 타깃에 그림 (클릭 피킹용)
//   타깃 전환·클리어·복사는 RecordFrame이 하고, 이 패스는 PSO + Draw만
class IdPass :public RenderPass
{
public:
	const wchar_t* Name() const override { return L"IdPadd"; }
	void Init(const RenderInitContext& ctx) override;
	void Execute(RenderContext& ctx) override;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
};