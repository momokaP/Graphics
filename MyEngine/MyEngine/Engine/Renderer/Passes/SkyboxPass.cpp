#include "Renderer/Passes/SkyboxPass.h"
#include "GeometryGenerator.h"
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

void SkyboxPass::Init(const RenderInitContext& ctx)
{
    MeshData box = GeometryGenerator::MakeBox();
    m_vb.CreateVertexBuffer(ctx.device, *ctx.upload, box.vertices, L"Skybox VB");
    m_ib.CreateIndexBuffer(ctx.device, *ctx.upload, box.indices, L"Skybox IB");

    m_object = hlsl::MakeObjectConstants(DirectX::SimpleMath::Matrix::CreateScale(20.0f));
}

bool SkyboxPass::CreatePipelines(const RenderInitContext& ctx)
{
    ShaderResult vs = ctx.shaders->Compile({ L"VertexShader.hlsl", ShaderTarget::VS });
    ShaderResult ps = ctx.shaders->Compile({ L"CubeMappingPixelShader.hlsl",  ShaderTarget::PS });
    if (!vs.ok || !ps.ok)
        return false;

    auto pso = PipelineBuilder(ctx.rootSignature)
        .VS(vs.bytecode).PS(ps.bytecode).InputLayout(kVertexLayout)
        .RenderTarget(kHdrFormat).DepthFormat(kDepthFormat)
        .Cull(D3D12_CULL_MODE_NONE).DepthWrite(false)
        .Build(ctx.device, L"Skybox PSO");
    if (!pso)
    {
        ctx.shaders->ReportError("Skybox PSO", "CreateGraphicsPipelineState failed (see D3D12 debuf output)");
        return false;
    }

    ReplacePso(ctx, m_pso, std::move(pso));
    return true;
}

void SkyboxPass::Execute(RenderContext& ctx)
{
    ID3D12GraphicsCommandList* cmd = ctx.cmd;
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootConstantBufferView(RP_Object, ctx.upload->Push(m_object));

    D3D12_VERTEX_BUFFER_VIEW vbv = m_vb.VertexView();
    D3D12_INDEX_BUFFER_VIEW  ibv = m_ib.IndexView();
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->IASetIndexBuffer(&ibv);
    cmd->DrawIndexedInstanced(m_ib.Count(), 1, 0, 0, 0);
    // 환경맵은 전역 t4 → 조율 쪽에서 이미 바인딩됨
}