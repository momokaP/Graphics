#include "stdafx.h"
#include "Renderer/Passes/UIPass.h"
#include "Renderer/RenderTypes.h"
#include "RHI/DescriptorHeap.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>

void UIPass::Init(const RenderInitContext& ctx)
{
    ImGui_ImplDX12_InitInfo info = {};
    info.Device = ctx.device;
    info.CommandQueue = ctx.queue;
    info.NumFramesInFlight = ctx.frameCount;
    info.RTVFormat = ctx.backBufferFormat;
    info.DSVFormat = kDepthFormat;
    info.SrvDescriptorHeap = ctx.srvHeap->Heap();
    info.UserData = ctx.srvHeap;
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* i, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
        {
            DescriptorHandle h = static_cast<DescriptorHeap*>(i->UserData)->Allocate();
            *cpu = h.cpu;
            *gpu = h.gpu;
        };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* i, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
        {
            static_cast<DescriptorHeap*>(i->UserData)->Free(cpu);
        };
    ImGui_ImplDX12_Init(&info);
}

void UIPass::Execute(RenderContext& ctx)
{
    // ⚠ ImGui는 내부에서 자기 루트 시그니처/PSO로 바꿈 → 이 패스 뒤에는 다른 패스를 두지 말 것
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ctx.cmd);
}

void UIPass::Shutdown()
{
    ImGui_ImplDX12_Shutdown();
}