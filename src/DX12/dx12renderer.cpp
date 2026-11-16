#include "dx12renderer.h"
#include <d3dcompiler.h>
#include <cstring>
#include <algorithm>
#include <iterator>
#include "../shadersource.h"
#include "../log.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace DirectX;

static const DXGI_FORMAT kbackFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
static const DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

static D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* r,
	D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = r;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	b.Transition.StateBefore = before;
	b.Transition.StateAfter = after;
	return b;
}

static bool Compile(const char* entry, const char* target, ComPtr<ID3DBlob>& out)
{
	ComPtr<ID3DBlob> err;
	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

	HRESULT hr = D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr,
		entry, target, flags, 0, &out, &err);
	if (FAILED(hr))
	{
		if (err)LogInfo((const char*)err->GetBufferPointer());
		LogError(entry, hr);
		return false;
	}
	return true;
}

bool DX12Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
{
	m_width = width;m_height = height;

	if (!CreateDeviceAndQueue())return false;
	if (!CreateSwapChain(hwnd))return false;
	if (!CreateDescriptorHeaps())return false;
	if (!CreateRendererTarget())return false;
	if (!CreateDepthBuffer())return false;
	if (!CreateCommandObject())return false;
	if (!CreateGpuTimer())return false;
	if (!CreateRootSignature())return false;
	if (!CreatePipelineState())return false;

	m_cmdList->Close();

	XMStoreFloat4x4(&m_view, XMMatrixIdentity());
	XMStoreFloat4x4(&m_proj, XMMatrixIdentity());
	LogInfo("DX12Renderer Ready");
	return true;
}


bool DX12Renderer::CreateDeviceAndQueue()
{
#ifdef _DEBUG
	ComPtr<ID3D12Debug>dbg;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer();
#endif // _DEBUG

	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));
	if (FAILED(hr)) { LogError("CreateDXGIFactory1", hr); return false; }

	hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr)) { LogError("D3D12CreateDevice", hr); return false;}
	
	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_queue));
	if (FAILED(hr)) { LogError("CreateCommandQueue", hr); return false; }
	LogInfo("DX12 Device and Queue Created");

	return true;
}

bool DX12Renderer::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.Width = m_width;
	sd.BufferDesc.Height = m_height;
	sd.BufferDesc.Format = kbackFormat;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = kframeCount;
	sd.OutputWindow = hwnd;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain> sc;
	HRESULT hr = m_factory->CreateSwapChain(m_queue.Get(), &sd, &sc);
	if (FAILED(hr)) { LogError("CreateSwapChain", hr); return false; }

	m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	hr = sc.As(&m_swapchain);
	if (FAILED(hr)) { LogError("SwapChain QueryInterface(IDXGISwapChain)", hr); return false;}

	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

	return true;
}

bool DX12Renderer::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtv{};
	rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv.NumDescriptors = kframeCount;
	HRESULT hr = m_device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&m_rtvHeap));
	if (FAILED(hr)) { LogError("CreateDescriptorHeap(RTV)", hr); return false;}

	D3D12_DESCRIPTOR_HEAP_DESC dsv{};
	dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsv.NumDescriptors = 1;
	hr = m_device->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&m_dsvHeap));
	if (FAILED(hr)) { LogError("CreateDiscriptorHeap(DSV)", hr);return false;}
	m_rtvInc = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	return true;
}

bool DX12Renderer::CreateRendererTarget()
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0;i < kframeCount;++i)
	{
		HRESULT hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_rtvs[i]));
		if (FAILED(hr)) { LogError("SwapChain::GetBuffer", hr); return false;}
		m_device->CreateRenderTargetView(m_rtvs[i].Get(), nullptr, h);
		h.ptr += m_rtvInc;
	}
	return true;
}

bool DX12Renderer::CreateDepthBuffer()
{
	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = m_width;
	rd.Height = m_height;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = kDepthFormat;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE cv{};
	cv.Format = kDepthFormat;
	cv.DepthStencil.Depth = 1.0f;

	HRESULT hr = m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_depth));
	if (FAILED(hr)) { LogError("CreateCommittedResource(depth)", hr); return false; }

	m_device->CreateDepthStencilView(m_depth.Get(), nullptr,
		m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	return true;
}

bool DX12Renderer::CreateCommandObject()
{
	HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator));
	if (FAILED(hr)) { LogError("CreateCommandAllocator", hr); return false; }

	hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocator.Get(), nullptr,
		IID_PPV_ARGS(&m_cmdList));
	if (FAILED(hr)) { LogError("CreateCommandList", hr); return false; }

	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(hr)) { LogError("CreateFance", hr); return false; }

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	return m_fenceEvent != nullptr;
}

bool DX12Renderer::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER p{};
	p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	p.Constants.ShaderRegister = 0;
	p.Constants.RegisterSpace = 0;
	p.Constants.Num32BitValues = sizeof(CBData) / 4;
	p.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 1;
	rs.pParameters = &p;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	
	ComPtr<ID3DBlob> blob, err;
	
	HRESULT hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
	if (FAILED(hr))
	{
		if (err) LogInfo((const char*)err->GetBufferPointer());
		LogError("D3D12SerializeRootSignature", hr);
		return false;
	}
	hr = m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSig));
	if (FAILED(hr)) { LogError("CreateRootSignature", hr); return false; }

	return true;
}

bool DX12Renderer::CreatePipelineState()
{
	ComPtr<ID3DBlob> vs, ps;
	if (!Compile("VSMain", "vs_5_0", vs)) return false;
	if (!Compile("PSMain", "ps_5_0", ps))return false;
	
	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = m_rootSig.Get();
	d.VS = { vs->GetBufferPointer(),vs->GetBufferSize() };
	d.PS = { ps->GetBufferPointer(),ps->GetBufferSize() };
	d.InputLayout = { layout,2 };

	d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	d.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	d.RasterizerState.DepthClipEnable = TRUE;

	auto& rt = d.BlendState.RenderTarget[0];
	rt.SrcBlend = D3D12_BLEND_ONE; rt.DestBlend = D3D12_BLEND_ZERO;
	rt.BlendOp = D3D12_BLEND_OP_ADD;
	rt.SrcBlendAlpha = D3D12_BLEND_ONE; rt.DestBlendAlpha = D3D12_BLEND_ZERO;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rt.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	d.DepthStencilState.DepthEnable = TRUE;
	d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	d.DepthStencilState.StencilReadMask = 0xFF;
	d.DepthStencilState.StencilWriteMask = 0xFF;
	D3D12_DEPTH_STENCILOP_DESC so{ D3D12_STENCIL_OP_KEEP,
	D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP ,D3D12_COMPARISON_FUNC_ALWAYS };
	d.DepthStencilState.FrontFace = so;
	d.DepthStencilState.BackFace = so;

	d.SampleMask=UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = kbackFormat;
	d.DSVFormat = kDepthFormat;
	d.SampleDesc.Count = 1;

	HRESULT hr = m_device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m_pso));
	if (FAILED(hr)) { LogError("CreateGraphicPiplineState", hr); return false;}

	return true;
}

bool DX12Renderer::WaitForGPU()
{
	if (m_gpuIdle)return true;
	if (!m_queue || !m_fence || !m_fenceEvent)return false;

	const UINT64 v = ++m_fenceValue;
	HRESULT hr = m_queue->Signal(m_fence.Get(), v);
	if (FAILED(hr))
	{
		LogError("CommandQueue::Signal(fence)", hr);
		return false;
	}
	const UINT64 completed = m_fence->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		LogError("Fence::GetCompletedValue", DXGI_ERROR_DEVICE_REMOVED);
		return false;
	}
	if (completed < v)
	{
		hr = m_fence->SetEventOnCompletion(v, m_fenceEvent);
		if (FAILED(hr))
		{
			LogError("Fence::SetEventOnCompletion", hr);
			return false;
		}
		if (WaitForSingleObject(m_fenceEvent, INFINITE) != WAIT_OBJECT_0)
		{
			LogError("WaitForSingleObject(fence)", HRESULT_FROM_WIN32(GetLastError()));
			return false;
		}
	}
	const UINT64 confirmed = m_fence->GetCompletedValue();
	if (confirmed == UINT64_MAX || confirmed < v)
	{
		LogError("Fence completion check", confirmed == UINT64_MAX ? DXGI_ERROR_DEVICE_REMOVED : E_FAIL);
		return false;
	}
	m_gpuIdle = true;
	return true;
}

bool DX12Renderer::CreateGpuTimer()
{
	HRESULT hr = m_queue->GetTimestampFrequency(&m_timestampFrequency);
	if (FAILED(hr) || m_timestampFrequency == 0)
	{
		LogError("GetTimeStampFrequency", FAILED(hr) ? hr : E_FAIL);
		return false;
	}

	D3D12_QUERY_HEAP_DESC query{};
	query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	query.Count = 2;

	hr = m_device->CreateQueryHeap(&query, IID_PPV_ARGS(&m_timestampHeap));
	if (FAILED(hr))
	{
		LogError("CreateQueryHeap(timestamp)", hr);
		return false;
	}

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC buffer{};
	buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	buffer.Width = sizeof(UINT64) * 2;
	buffer.Height = 1;
	buffer.DepthOrArraySize = 1;
	buffer.MipLevels = 1;
	buffer.SampleDesc.Count = 1;
	buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = m_device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&buffer,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_timestampReadback));

	if (FAILED(hr))
	{
		LogError("CreateCommittedResource(timestamp)", hr);
		return false;
	}

	return true;
}

void DX12Renderer::ReadGpuTimer()
{
	void* data = nullptr;
	D3D12_RANGE readRange{ 0, sizeof(UINT64) * 2 };

	HRESULT hr = m_timestampReadback->Map(0, &readRange, &data);
	if (FAILED(hr))
	{
		m_gpuMs = -1.0;
		return;
	}

	const UINT64* timestamps = static_cast<const UINT64*>(data);

	if (timestamps[1] >= timestamps[0])
	{
		m_gpuMs = double(timestamps[1] - timestamps[0])
			* 1000.0 / double(m_timestampFrequency);
	}
	else m_gpuMs = -1.0;

	D3D12_RANGE writtenRange{ 0,0 };
	m_timestampReadback->Unmap(0, &writtenRange);
}


void DX12Renderer::Resize(uint32_t width, uint32_t height)
{
	if (!m_swapchain || width == 0 || height == 0 || m_frameActive)return;
	if (!WaitForGPU())return;

	for (auto& r : m_rtvs)r.Reset();

	m_depth.Reset();

	HRESULT hr = m_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) { LogError("ResizeBuffers", hr); return; }

	m_width = width;m_height = height;
	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

	CreateRendererTarget();
	CreateDepthBuffer();
}

void DX12Renderer::Shutdown()
{
	const bool idle = WaitForGPU();
	if (idle)
	{
		for (UploadPage& page : m_uploadPages)
		{
			if (page.mapped)
			{
				page.resource->Unmap(0, nullptr);
				page.mapped = nullptr;
			}
		}
	}
	if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }

	m_meshes.clear();
	m_uploadPages.clear();
	m_frameActive = false;
	m_gpuIdle = true;
	m_pso.Reset(); m_rootSig.Reset();
	m_cmdList.Reset(); m_allocator.Reset();
	m_depth.Reset();
	for (auto& r : m_rtvs)r.Reset();
	m_dsvHeap.Reset(); m_rtvHeap.Reset();
	m_swapchain.Reset();
	m_fence.Reset();
	m_queue.Reset();
	m_timestampReadback.Reset();
	m_timestampHeap.Reset();
	m_timestampFrequency = 0;
	m_gpuMs = -1.0;
	m_device.Reset();
	m_factory.Reset();
}

bool DX12Renderer::CreateUploadPage(UINT64 minimumSize)
{
	constexpr UINT64 pageSize = 4ull * 1024ull * 1024ull;
	if (minimumSize > UINT64_MAX - 15)return false;
	const UINT64 size = (std::max)(pageSize, (minimumSize + 15ull) & ~15ull);

	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	UploadPage page;
	HRESULT hr = m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&page.resource));
	if (FAILED(hr))
	{
		LogError("CreateCommittedResource(upload page)", hr);
		return false;
	}
	void* mapped = nullptr;
	D3D12_RANGE noRead{ 0,0 };
	hr = page.resource->Map(0, &noRead, &mapped);
	if (FAILED(hr))
	{
		LogError("Resource::Map(upload page)", hr);
		return false;
	}
	page.mapped = static_cast<uint8_t*>(mapped);
	page.size = size;
	page.freeRanges.emplace(0, size);
	m_uploadPages.push_back(std::move(page));
	return true;
}

bool DX12Renderer::AllocateMeshStorage(UINT64 size, size_t& pageIndex, UINT64& offset)
{
	if (size == 0 || size > UINT64_MAX - 15)return false;
	size = (size + 15ull) & ~15ull;
	auto findRange = [&](size_t index) -> bool
	{
		auto& ranges = m_uploadPages[index].freeRanges;
		for (auto it = ranges.begin(); it != ranges.end(); ++it)
		{
			if (it->second < size)continue;
			offset = it->first;
			const UINT64 remaining = it->second - size;
			const UINT64 nextOffset = it->first + size;
			ranges.erase(it);
			if (remaining != 0)ranges.emplace(nextOffset, remaining);
			pageIndex = index;
			return true;
		}
		return false;
	};
	for (size_t i = 0; i < m_uploadPages.size(); ++i)
		if (findRange(i))return true;
	if (!CreateUploadPage(size))return false;
	return findRange(m_uploadPages.size() - 1);
}

void DX12Renderer::FreeMeshAllocation(const GpuMesh& mesh)
{
	if (mesh.pageIndex >= m_uploadPages.size() || mesh.allocationSize == 0)return;
	auto& ranges = m_uploadPages[mesh.pageIndex].freeRanges;
	UINT64 offset = mesh.allocationOffset;
	UINT64 size = mesh.allocationSize;
	auto next = ranges.lower_bound(offset);
	if (next != ranges.begin())
	{
		auto previous = std::prev(next);
		if (previous->first + previous->second == offset)
		{
			offset = previous->first;
			size += previous->second;
			ranges.erase(previous);
		}
	}
	next = ranges.lower_bound(offset);
	if (next != ranges.end() && offset + size == next->first)
	{
		size += next->second;
		ranges.erase(next);
	}
	ranges.emplace(offset, size);
}

void DX12Renderer::UploadMesh(uint32_t id, const Mesh& mesh)
{
	if (m_frameActive || !WaitForGPU())return;
	if (mesh.vertices.empty() || mesh.indices.empty() ||
		mesh.vertices.size() > UINT_MAX / sizeof(Vertex) ||
		mesh.indices.size() > UINT_MAX / sizeof(uint32_t))
	{
		LogInfo("UploadMesh skipped: empty mesh or buffer size overflow");
		return;
	}
	const UINT vbsize = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
	const UINT ibsize = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
	const UINT64 indexOffset = (UINT64(vbsize) + 3ull) & ~3ull;
	if (indexOffset > UINT64_MAX - ibsize || indexOffset + ibsize > UINT64_MAX - 15ull)
	{
		LogInfo("UploadMesh skipped: allocation size overflow");
		return;
	}
	const UINT64 allocationSize = (indexOffset + ibsize + 15ull) & ~15ull;
	size_t pageIndex = 0;
	UINT64 allocationOffset = 0;
	if (!AllocateMeshStorage(allocationSize, pageIndex, allocationOffset))
	{
		LogInfo("UploadMesh skipped: upload page allocation failed");
		return;
	}

	UploadPage& page = m_uploadPages[pageIndex];
	memcpy(page.mapped + allocationOffset, mesh.vertices.data(), vbsize);
	memcpy(page.mapped + allocationOffset + indexOffset, mesh.indices.data(), ibsize);

	GpuMesh g;
	g.vbv = { page.resource->GetGPUVirtualAddress() + allocationOffset, vbsize, sizeof(Vertex) };
	g.ibv = { page.resource->GetGPUVirtualAddress() + allocationOffset + indexOffset, ibsize, DXGI_FORMAT_R32_UINT };
	g.indexCount = static_cast<UINT>(mesh.indices.size());
	g.pageIndex = pageIndex;
	g.allocationOffset = allocationOffset;
	g.allocationSize = allocationSize;

	auto existing = m_meshes.find(id);
	if (existing != m_meshes.end())
	{
		const GpuMesh old = existing->second;
		existing->second = g;
		FreeMeshAllocation(old);
	}
	else m_meshes.emplace(id, g);
}

void DX12Renderer::RemoveMesh(uint32_t id)
{
	if (m_frameActive || !WaitForGPU())return;
	auto it = m_meshes.find(id);
	if (it == m_meshes.end())return;
	FreeMeshAllocation(it->second);
	m_meshes.erase(it);
}

void DX12Renderer::SetCamera(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
{
	XMStoreFloat4x4(&m_view, XMMatrixTranspose(XMLoadFloat4x4(&view)));
	XMStoreFloat4x4(&m_proj, XMMatrixTranspose(XMLoadFloat4x4(&proj)));
}

void DX12Renderer::BeginFrame()
{
	m_frameActive = false;
	if (!WaitForGPU())return;
	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

	HRESULT hr = m_allocator->Reset();
	if (FAILED(hr)) { LogError("CommandAllocator::Reset", hr); return; }
	hr = m_cmdList->Reset(m_allocator.Get(), m_pso.Get());
	if (FAILED(hr)) { LogError("CommandList::Reset", hr); return; }
	m_frameActive = true;
	m_cmdList->EndQuery(
		m_timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

	D3D12_RESOURCE_BARRIER toRT = Transition(m_rtvs[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_cmdList->ResourceBarrier(1, &toRT);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtv.ptr += SIZE_T(m_frameIndex) * m_rtvInc;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	const float clear[4] = { 0.08f,0.08f,0.12f,1.0f };
	m_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);
	m_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	D3D12_VIEWPORT vp{ 0,0,(float)m_width,(float)m_height,0.0f,1.0f };
	D3D12_RECT sc{ 0,0,(LONG)m_width ,(LONG)m_height};
	m_cmdList->RSSetViewports(1, &vp);
	m_cmdList->RSSetScissorRects(1, &sc);

	m_cmdList->SetGraphicsRootSignature(m_rootSig.Get());
	m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DX12Renderer::Draw(uint32_t meshid, const DirectX::XMFLOAT4X4& world)
{
	if (!m_frameActive)return;
	auto it = m_meshes.find(meshid);
	if (it == m_meshes.end())return;
	const GpuMesh& g = it->second;

	CBData cb;
	XMStoreFloat4x4(&cb.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
	cb.view = m_view;
	cb.proj = m_proj;

	m_cmdList->SetGraphicsRoot32BitConstants(0, sizeof(CBData) / 4, &cb, 0);

	m_cmdList->IASetVertexBuffers(0, 1, &g.vbv);
	m_cmdList->IASetIndexBuffer(&g.ibv);
	m_cmdList->DrawIndexedInstanced(g.indexCount, 1, 0, 0, 0);
}

void DX12Renderer::EndFrame()
{
	if (!m_frameActive)return;
	m_frameActive = false;
	D3D12_RESOURCE_BARRIER toPresent = Transition(
		m_rtvs[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	m_cmdList->ResourceBarrier(1, &toPresent);

	m_cmdList->EndQuery(
		m_timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

	m_cmdList->ResolveQueryData(
		m_timestampHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		0, 2,
		m_timestampReadback.Get(),
		0);

	HRESULT hr = m_cmdList->Close();
	if (FAILED(hr)) { LogError("CommandList::Close", hr); return; }

	ID3D12CommandList* lists[] = { m_cmdList.Get() };
	m_gpuIdle = false;
	m_queue->ExecuteCommandLists(1, lists);

	// SyncInterval 1 follows the refresh rate of the display showing this window.
	hr = m_swapchain->Present(1, 0);
	if (FAILED(hr))LogError("SwapChain::Present", hr);
	if (WaitForGPU())
	{
		// Read timestamps only after the resolve has completed.
		ReadGpuTimer();
	}
	else m_gpuMs = -1.0;
}
