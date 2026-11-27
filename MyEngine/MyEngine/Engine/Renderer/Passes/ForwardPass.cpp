#include "Renderer/Passes/ForwardPass.h"
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
#include "Engine/RHI/ShaderCompiler.h"

void ForwardPass::Init(const RenderInitContext& ctx)
{

}

bool ForwardPass::CreatePipelines(const RenderInitContext& ctx)
{
    ShaderResult vs = ctx.shaders->Compile({ L"VertexShader.hlsl", ShaderTarget::VS });
    ShaderResult ps = ctx.shaders->Compile({ L"PixelShader.hlsl",  ShaderTarget::PS });
    if (!vs.ok || !ps.ok)
        return false;

    auto pso = PipelineBuilder(ctx.rootSignature)
        .VS(vs.bytecode).PS(ps.bytecode).InputLayout(kVertexLayout)
        .RenderTarget(kHdrFormat).DepthFormat(kDepthFormat)
        .Cull(D3D12_CULL_MODE_NONE)
        .TryBuild(ctx.device, L"Forward PSO");
    if (!pso)
    {
        ctx.shaders->ReportError("Forward PSO", "CreateGraphicsPipelineState failed (see D3D12 debuf output)");
        return false;
    }

    ReplacePso(ctx, m_pso, std::move(pso));
    return true;
}

void ForwardPass::Execute(RenderContext& ctx)
{
    ID3D12GraphicsCommandList* cmd = ctx.cmd;
    const RenderScene& scene = *ctx.scene;

    cmd->SetPipelineState(m_pso.Get());

    for (const DrawItem& d : scene.draws)
    {
        cmd->SetGraphicsRootConstantBufferView(RP_Object, ctx.upload->Push(d.object));
        cmd->SetGraphicsRootConstantBufferView(RP_Material, ctx.upload->Push(d.material));
        cmd->SetGraphicsRootDescriptorTable(RP_Tex0, d.albedo->srv.gpu);

        D3D12_VERTEX_BUFFER_VIEW vbv = d.vertexBuffer->VertexView();
        D3D12_INDEX_BUFFER_VIEW  ibv = d.indexBuffer->IndexView();
        cmd->IASetVertexBuffers(0, 1, &vbv);
        cmd->IASetIndexBuffer(&ibv);
        cmd->DrawIndexedInstanced(d.indexBuffer->Count(), 1, 0, 0, 0);
    }
}