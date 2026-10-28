#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <deque>

class DeferredReleaseQueue
{
public:
    // fenceValue: "이 값까지 GPU가 끝나면 지워도 됨"
    void Push(Microsoft::WRL::ComPtr<ID3D12Pageable> object, UINT64 fenceValue)
    {
        m_items.push_back({ std::move(object), fenceValue });
    }

    // 매 프레임 호출: 이미 GPU가 끝낸 항목부터 실제로 Release
    void Collect(UINT64 completedFenceValue)
    {
        while (!m_items.empty() && m_items.front().fenceValue <= completedFenceValue)
            m_items.pop_front();          // ComPtr 소멸 → Release
    }

    void Flush() { m_items.clear(); }     // GPU가 비어 있을 때(종료) 전부 해제

private:
    struct Item { Microsoft::WRL::ComPtr<ID3D12Pageable> object; UINT64 fenceValue; };
    std::deque<Item> m_items;             // 펜스 값이 증가하는 순서로 쌓이므로 앞에서부터 확인
};