#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <DirectXMath.h>
#include "../IRenderer.h"

using Microsoft::WRL::ComPtr;

class DX12Renderer : public IRenderer
{
public:
	bool Initialize(HWND hwnd, uint32_t width, uint32_t height)override;
	void Resize(uint32_t width, uint32_t height) override;
	void Shutdown() override;

	void UploadMesh(uint32_t id, const Mesh& mesh) override;
	void RemoveMesh(uint32_t id) override;

	void SetCamera(const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& proj)override;

	void BeginFrame() override;
	void Draw(uint32_t meshid, const DirectX::XMFLOAT4X4& world)override;
	void EndFrame() override;

	double GpuMilliseconds() const override { return m_gpuMs; }

private:
	static constexpr UINT kframeCount = 2;

	struct GpuMesh
	{
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		UINT indexCount = 0;
		size_t pageIndex = 0;
		UINT64 allocationOffset = 0;
		UINT64 allocationSize = 0;
	};

	struct UploadPage
	{
		ComPtr<ID3D12Resource> resource;
		uint8_t* mapped = nullptr;
		UINT64 size = 0;
		std::map<UINT64, UINT64> freeRanges;
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
	bool WaitForGPU();
	void FreeMeshAllocation(const GpuMesh& mesh);

	bool CreateGpuTimer();
	void ReadGpuTimer();

	bool CreateUploadPage(UINT64 minimumSize);
	bool AllocateMeshStorage(UINT64 size, size_t& pageIndex, UINT64& offset);
	
	ComPtr<IDXGIFactory4>					m_factory;
	ComPtr<ID3D12Device>					m_device;
	ComPtr<ID3D12CommandQueue>				m_queue;
	ComPtr<IDXGISwapChain3>					m_swapchain;

	ComPtr<ID3D12DescriptorHeap>			m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap>			m_dsvHeap;
	ComPtr<ID3D12Resource>					m_rtvs[kframeCount];
	ComPtr<ID3D12Resource>					m_depth;
	UINT									m_rtvInc = 0;

	ComPtr<ID3D12CommandAllocator>			m_allocator;
	ComPtr<ID3D12GraphicsCommandList>		m_cmdList;
	ComPtr<ID3D12RootSignature>				m_rootSig;
	ComPtr<ID3D12PipelineState>				m_pso;

	ComPtr<ID3D12QueryHeap>					m_timestampHeap;
	ComPtr<ID3D12Resource>					m_timestampReadback;

	ComPtr<ID3D12Fence>						m_fence;
	UINT64									m_fenceValue = 0;
	HANDLE									m_fenceEvent = nullptr;
	bool m_gpuIdle = true;
	bool m_frameActive = false;

	std::unordered_map<uint32_t, GpuMesh>	m_meshes;
	std::vector<UploadPage> m_uploadPages;

	XMFLOAT4X4								m_view{};
	XMFLOAT4X4								m_proj{};
	uint32_t								m_width = 0, m_height = 0;
	UINT									m_frameIndex = 0;

	UINT64									m_timestampFrequency = 0;
	double									m_gpuMs = -1.0;


};
