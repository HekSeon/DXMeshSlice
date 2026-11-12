#include "dx12renderer.h"
#include <d3dcompiler.h>
#include <cstring>
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
	return false;
}

void DX12Renderer::Resize(uint32_t width, uint32_t height)
{
}

void DX12Renderer::Shutdown()
{
}

void DX12Renderer::UploadMesh(uint32_t id, const Mesh& mesh)
{
}

void DX12Renderer::RemoveMesh(uint32_t id)
{
}

void DX12Renderer::SetCamera(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
{
}

void DX12Renderer::BeginFrame()
{
}

void DX12Renderer::Draw(uint32_t meshid, const DirectX::XMFLOAT4X4& world)
{
}

void DX12Renderer::EndFrame()
{
}
