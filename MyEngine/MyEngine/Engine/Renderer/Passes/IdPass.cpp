#include "stdafx.h"
#include "Renderer/Passes/IdPass.h"
#include <cstddef>
#include "Renderer/RenderTypes.h"
#include "Renderer/RenderScene.h"
#include "RHI/PipelineBuilder.h"
#include "RHI/LinearUploadAllocator.h"
#include "RHI/GpuBuffer.h"
#include "DXSampleHelper.h"
#include "Paths.h"
#include "Vertex.h"
#include "Engine/RHI/ShaderCompiler.h"

namespace
{
	constexpr UINT kObjectIdOffset = offsetof(hlsl::DebugConstants, objectId) / 4; // = 6 (DWORD 단위)
}

void IdPass::Init(const RenderInitContext& ctx)
{
	
}

bool IdPass::CreatePipelines(const RenderInitContext& ctx)
{
	ShaderResult vs = ctx.shaders->Compile({ L"VertexShader.hlsl", ShaderTarget::VS });
	ShaderResult ps = ctx.shaders->Compile({ L"IdPS.hlsl",  ShaderTarget::PS });
	if (!vs.ok || !ps.ok)
		return false;

	auto pso = PipelineBuilder(ctx.rootSignature)
		.VS(vs.bytecode).PS(ps.bytecode).InputLayout(kVertexLayout)
		.RenderTarget(DXGI_FORMAT_R32_UINT).DepthFormat(kDepthFormat)
		.Cull(D3D12_CULL_MODE_NONE)
		.DepthTest(true, D3D12_COMPARISON_FUNC_LESS) // 일반 깊이 테스트
		.DepthWrite(true) // IdPass 안에서 가장 가까운 것이 이기도록
		.Build(ctx.device, L"Id PSO");
	if (!pso)
	{
		ctx.shaders->ReportError("Id PSO", "CreateGraphicsPipelineState failed (see D3D12 debuf output)");
		return false;
	}

	ReplacePso(ctx, m_pso, std::move(pso));
	return true;
}

void IdPass::Execute(RenderContext& ctx)
{
	ID3D12GraphicsCommandList* cmd = ctx.cmd;
	cmd->SetPipelineState(m_pso.Get());

	for (const DrawItem& d : ctx.scene->draws)
	{
		cmd->SetGraphicsRootConstantBufferView(RP_Object, ctx.upload->Push(d.object));
		cmd->SetGraphicsRoot32BitConstant(RP_Debug, d.entityId, kObjectIdOffset);
	
		D3D12_VERTEX_BUFFER_VIEW vbv = d.vertexBuffer->VertexView();
		D3D12_INDEX_BUFFER_VIEW ibv = d.indexBuffer->IndexView();
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->IASetIndexBuffer(&ibv);
		cmd->DrawIndexedInstanced(d.indexBuffer->Count(), 1, 0, 0, 0);
	}
}
