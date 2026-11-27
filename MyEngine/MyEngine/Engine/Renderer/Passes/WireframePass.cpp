#include <algorithm>
#include "stdafx.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/RenderScene.h"
#include "RHI/PipelineBuilder.h"
#include "RHI/LinearUploadAllocator.h"
#include "RHI/Texture.h"
#include "RHI/GpuBuffer.h"
#include "DXSampleHelper.h"     // LoadShader, ThrowIfFailed
#include "Paths.h"
#include "Vertex.h"
#include "WireframePass.h"
#include "Engine/RHI/ShaderCompiler.h"

void WireframePass::Init(const RenderInitContext& ctx)
{
    
}

bool WireframePass::CreatePipelines(const RenderInitContext& ctx)
{
    ShaderResult vs = ctx.shaders->Compile({ L"VertexShader.hlsl", ShaderTarget::VS });
    ShaderResult ps = ctx.shaders->Compile({ L"WireFramePixelShader.hlsl",  ShaderTarget::PS });
    if (!vs.ok || !ps.ok)
        return false;

    auto pso = PipelineBuilder(ctx.rootSignature)
        .VS(vs.bytecode).PS(ps.bytecode).InputLayout(kVertexLayout)
        .RenderTarget(kHdrFormat).DepthFormat(kDepthFormat)
        .Cull(D3D12_CULL_MODE_NONE)
        .FillMode(D3D12_FILL_MODE_WIREFRAME)
        .DepthTest(true, D3D12_COMPARISON_FUNC_LESS_EQUAL)
        .DepthWrite(false)
        .DepthBias(-100, 0.0f)
        .Build(ctx.device, L"Wireframe PSO");
    if (!pso)
    {
        ctx.shaders->ReportError("Wireframe PSO", "CreateGraphicsPipelineState failed (see D3D12 debuf output)");
        return false;
    }

    ReplacePso(ctx, m_pso, std::move(pso));
    return true;
}

void WireframePass::Execute(RenderContext& ctx)
{
    ID3D12GraphicsCommandList* cmd = ctx.cmd;
    const RenderScene& scene = *ctx.scene;

    cmd->SetPipelineState(m_pso.Get());
    
    hlsl::DebugConstants dc{};
    dc.tint = scene.tint;
    cmd->SetGraphicsRoot32BitConstants(RP_Debug, kDebugConstantCount, &dc, 0);

    for (const DrawItem& d : scene.draws)
    {
        cmd->SetGraphicsRootConstantBufferView(RP_Object, ctx.upload->Push(d.object));

        D3D12_VERTEX_BUFFER_VIEW vbv = d.vertexBuffer->VertexView();
        D3D12_INDEX_BUFFER_VIEW  ibv = d.indexBuffer->IndexView();
        cmd->IASetVertexBuffers(0, 1, &vbv);
        cmd->IASetIndexBuffer(&ibv);
        cmd->DrawIndexedInstanced(d.indexBuffer->Count(), 1, 0, 0, 0);
    }
}

void WireframePass::Shutdown()
{

}
