#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <unordered_map>
#include <DirectXMath.h>
#include "../IRenderer.h"

using Microsoft::WRL::ComPtr;

class DX12Renderer : public IRenderer
{
public:
	bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
	void Resize(uint32_t width, uint32_t height) override;
	void Shutdown() override;

	void UploadMesh(uint32_t id, const Mesh& mesh) override;
	void RemoveMesh(uint32_t id) override;

	void SetCamera(const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& proj)override;

	void BeginFrame() override;
	void Draw(uint32_t meshid, const DirectX::XMFLOAT4X4& world)override;
	void EndFrame() override;

private:
	static constexpr UINT kframeCount = 2;

	struct GpuMesh
	{
		ComPtr<ID3D12Resource>		vb;
		ComPtr<ID3D12Resource>		ib;
		D3D12_VERTEX_BUFFER_VIEW	vbv{};
		D3D12_INDEX_BUFFER_VIEW		ibv{};
		UINT						indexCount = 0;
	};

	struct CBData
	{
		XMFLOAT4X4 world;
		XMFLOAT4X4 view;
		XMFLOAT4X4 proj;
	};

	bool CreateDeviceAndQueue();
	bool CreateSwapChain(HWND);
	bool CreateDescriptorHeaps();
	bool CreateRendererTarget();
	bool CreateDepthBuffer();
	bool CreateCommandObject();
	bool CreateRootSignature();
	bool CreatePipelineState();
	void WaitForGPU();
	ComPtr<ID3D12Resource> CreateUploadBuffer(const void* data, UINT64 size);
	
	ComPtr<IDXGIFactory>					m_factory;
	ComPtr<ID3D12Device>					m_device;
	ComPtr<ID3D12CommandQueue>				m_queue;
	ComPtr<IDXGISwapChain>					m_swapchain;

	ComPtr<ID3D12DescriptorHeap>			m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap>			m_dtvHeap;
	ComPtr<ID3D12Resource>					m_rtvs[kframeCount];
	ComPtr<ID3D12Resource>					m_depth;
	UINT									m_rtvInc = 0;

	ComPtr<ID3D12CommandAllocator>			m_allocator;
	ComPtr<ID3D12GraphicsCommandList>		m_cmdList;
	ComPtr<ID3D12RootSignature>				m_rootSig;
	ComPtr<ID3D12PipelineState>				m_pso;

	ComPtr<ID3D12Fence>						m_fence;
	UINT64									m_fenceValue = 0;
	HANDLE									m_fenceEvent = nullptr;

	std::unordered_map<uint32_t, GpuMesh>	m_meshes;

	XMFLOAT4X4								m_view{};
	XMFLOAT4X4								m_proj{};
	uint32_t								m_width = 0, m_height = 0;
	UINT									m_frameIndex = 0;
};