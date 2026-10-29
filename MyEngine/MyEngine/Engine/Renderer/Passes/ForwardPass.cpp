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

void ForwardPass::Init(const RenderInitContext& ctx)
{
    auto vs = LoadShader(Paths::Shader(L"VertexShader.cso"));
    auto ps = LoadShader(Paths::Shader(L"PixelShader.cso"));
    m_pso = PipelineBuilder(ctx.rootSignature)
        .VS(vs).PS(ps).InputLayout(kVertexLayout)
        .RenderTarget(kHdrFormat).DepthFormat(kDepthFormat)
        .Cull(D3D12_CULL_MODE_NONE)
        .Build(ctx.device, L"Forward PSO");
}

void ForwardPass::Execute(RenderContext& ctx)
{
    ID3D12GraphicsCommandList* cmd = ctx.cmd;
    const RenderScene& scene = *ctx.scene;

    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootConstantBufferView(RP_Material, ctx.upload->Push(scene.material));

    for (const DrawItem& d : scene.draws)
    {
        cmd->SetGraphicsRootConstantBufferView(RP_Object, ctx.upload->Push(d.object));
        cmd->SetGraphicsRootDescriptorTable(RP_Tex0, d.albedo->srv.gpu);

        D3D12_VERTEX_BUFFER_VIEW vbv = d.vertexBuffer->VertexView();
        D3D12_INDEX_BUFFER_VIEW  ibv = d.indexBuffer->IndexView();
        cmd->IASetVertexBuffers(0, 1, &vbv);
        cmd->IASetIndexBuffer(&ibv);
        cmd->DrawIndexedInstanced(d.indexBuffer->Count(), 1, 0, 0, 0);
    }
}