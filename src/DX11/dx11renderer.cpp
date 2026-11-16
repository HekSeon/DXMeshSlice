#include "dx11renderer.h"
#include <d3dcompiler.h>
#include <cstring>
#include "../log.h"
#include "../shadersource.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;


bool DX11Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
{
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 2;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		flags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&m_swapChain,
		&m_device,
		nullptr,
		&m_context);
	if (FAILED(hr))
	{
		LogError("D3D11CreateDeviceAndSwapChain", hr);
		return false;
	}
	LogInfo("DX11 Device and swapchain created");

	m_width = width, m_height = height;
	CreateRendererTarget();
	CreateDepthBuffer();
	if (!CreateShaders())			return false;
	if (!CreatePipelineStates())	return false;
	if (!CreateGpuTimers())		return false;

	XMStoreFloat4x4(&m_view, XMMatrixIdentity());
	XMStoreFloat4x4(&m_proj, XMMatrixIdentity());

	return true;
}

void DX11Renderer::CreateRendererTarget()//<==========renderer target creating
{
	ComPtr<ID3D11Texture2D> backBuffer;
	m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv);
}

void DX11Renderer::CreateDepthBuffer()
{
	D3D11_TEXTURE2D_DESC	d{};
	d.Width					= m_width;
	d.Height = m_height;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	HRESULT hr = m_device->CreateTexture2D(&d, nullptr, &m_depthTex);
	if (FAILED(hr)) { LogError("CreateTexture2D(depth)", hr); return; }
	hr = m_device->CreateDepthStencilView(m_depthTex.Get(), nullptr, &m_dsv);
	if (FAILED(hr)) LogError("CreateDepthStencilView", hr);
}

static bool Compile(const char* entry, const char* target, ComPtr<ID3DBlob>& out)
{
	ComPtr<ID3DBlob>err;
	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

	HRESULT hr = D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr,
		entry, target, flags, 0, &out, &err);

	if (FAILED(hr))
	{
		if (err) LogInfo((const char*)err->GetBufferPointer());
		LogError(entry, hr);
		return false;
	}
	return true;
}

bool DX11Renderer::CreateShaders()
{
	ComPtr<ID3DBlob> vsBlob, psBlob;
	if (!Compile("VSMain", "vs_5_0", vsBlob)) return false;
	if (!Compile("PSMain", "ps_5_0", psBlob)) return false;
	HRESULT hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
		nullptr, &m_vs);
	if (FAILED(hr)) { LogError("CreateVertexShader", hr); return false;}
	hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
		nullptr, &m_ps);
	if (FAILED(hr)) { LogError("CreatePixelShader", hr); return false; }

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
	if (FAILED(hr)) { LogError("CreateInputLayout", hr); return false; }
	return true;
}

bool DX11Renderer::CreatePipelineStates()
{
	D3D11_BUFFER_DESC cb{};
	cb.ByteWidth = sizeof(CBData);
	cb.Usage = D3D11_USAGE_DEFAULT;
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HRESULT hr = m_device->CreateBuffer(&cb, nullptr, &m_cb);
	if (FAILED(hr)) { LogError("CreateBuffer(cb)", hr); return false; }

	D3D11_RASTERIZER_DESC rd{};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = TRUE;
	hr = m_device->CreateRasterizerState(&rd, &m_raster);
	if (FAILED(hr)) { LogError("CreateRasterizerState", hr); return false; };
	return true;

}

bool DX11Renderer::CreateGpuTimers()
{
	for (GpuTimer& timer : m_timers)
	{
		D3D11_QUERY_DESC desc{};
		desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		HRESULT hr = m_device->CreateQuery(&desc, &timer.disjoint);
		if (FAILED(hr)) { LogError("CreateQuery(disjoint)", hr); return false; }
		desc.Query = D3D11_QUERY_TIMESTAMP;
		hr = m_device->CreateQuery(&desc, &timer.start);
		if (FAILED(hr)) { LogError("CreateQuery(start)", hr); return false; }
		hr = m_device->CreateQuery(&desc, &timer.end);
		if (FAILED(hr)) { LogError("CreateQuery(end)", hr); return false; }
	}
	return true;
}

void DX11Renderer::ReadGpuTimers()
{
	// Hazir olmayan sonucu bekleme ve kullanimdaki sorguyu tekrar yazma.
	while (m_timerPending > 0)
	{
		GpuTimer& timer = m_timers[m_timerRead];
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
		UINT64 start = 0, end = 0;
		HRESULT hr = m_context->GetData(timer.disjoint.Get(), &disjoint,
			sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		if (hr == S_FALSE)return;
		if (SUCCEEDED(hr) && !disjoint.Disjoint && disjoint.Frequency != 0)
		{
			hr = m_context->GetData(timer.start.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (hr == S_FALSE)return;
			if (SUCCEEDED(hr))
			{
				hr = m_context->GetData(timer.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
				if (hr == S_FALSE)return;
			}
			m_gpuMs = SUCCEEDED(hr) && end >= start
				? double(end - start) * 1000.0 / double(disjoint.Frequency) : -1.0;
		}
		else m_gpuMs = -1.0;
		if (FAILED(hr))LogError("GetData(timestamp)", hr);
		m_timerRead = (m_timerRead + 1) % kTimerCount;
		--m_timerPending;
	}
}

void DX11Renderer::Resize(uint32_t width, uint32_t height) //<====Resizing window
{
	if (!m_swapChain || width == 0 || height == 0) return;
	m_context->OMSetRenderTargets(0, nullptr, nullptr);
	m_rtv.Reset();
	m_dsv.Reset();
	m_depthTex.Reset();
	m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	m_width = width;m_height = height;
	CreateRendererTarget();
	CreateDepthBuffer();
}

void DX11Renderer::Shutdown() //<=======Clear
{
	for (GpuTimer& timer : m_timers)
	{
		timer.disjoint.Reset();timer.start.Reset();timer.end.Reset();
	}
	m_timerRead = m_timerWrite = m_timerPending = 0;
	m_timerActive = false;
	m_gpuMs = -1.0;
	m_meshes.clear();
	m_raster.Reset(); m_cb.Reset(); m_layout.Reset();
	m_ps.Reset(); m_vs.Reset();
	m_dsv.Reset(); m_depthTex.Reset();
	m_rtv.Reset();
	m_swapChain.Reset();
	m_context.Reset();
	m_device.Reset();
}

void DX11Renderer::BeginFrame() //<=====FrameCount start
{
	ReadGpuTimers();
	m_timerActive = m_timerPending < kTimerCount;
	if (m_timerActive)
	{
		GpuTimer& timer = m_timers[m_timerWrite];
		m_context->Begin(timer.disjoint.Get());
		m_context->End(timer.start.Get());
	}

	const float clear[4] = { 0.08f,0.08f,0.12f,1.0f };
	m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
	m_context->ClearRenderTargetView(m_rtv.Get(), clear);
	m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	D3D11_VIEWPORT vp{ 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
	m_context->RSSetViewports(1, &vp);
	m_context->IASetInputLayout(m_layout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->VSSetShader(m_vs.Get(), nullptr, 0);
	m_context->PSSetShader(m_ps.Get(), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_cb.GetAddressOf());
	m_context->RSSetState(m_raster.Get());
}

void DX11Renderer::EndFrame() //<=====framecount end
{
	if (m_timerActive)
	{
		GpuTimer& timer = m_timers[m_timerWrite];
		m_context->End(timer.end.Get());
		m_context->End(timer.disjoint.Get());
		m_timerWrite = (m_timerWrite + 1) % kTimerCount;
		++m_timerPending;
		m_timerActive = false;
	}
	// SyncInterval 1 follows the refresh rate of the display showing this window.
	HRESULT hr = m_swapChain->Present(1, 0);
	if (FAILED(hr))LogError("SwapChain::Present", hr);
}

void DX11Renderer::UploadMesh(uint32_t id, const Mesh& mesh) 
{
	GpuMesh g;
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = UINT(mesh.vertices.size()*sizeof(Vertex));
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA sd{ mesh.vertices.data() };
	HRESULT hr = m_device->CreateBuffer(&bd, &sd, &g.vb);
	if (FAILED(hr)) { LogError("CreateBuffer(vb)", hr); return; }

	bd.ByteWidth = UINT(mesh.indices.size() * sizeof(uint32_t));
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	sd.pSysMem = mesh.indices.data();
	hr = m_device->CreateBuffer(&bd, &sd, &g.ib);
	if (FAILED(hr)) { LogError("CreateBuffer(ib)", hr); return; }

	g.indexCount = UINT(mesh.indices.size());
	m_meshes[id] = std::move(g);
}
void DX11Renderer::RemoveMesh(uint32_t id) { m_meshes.erase(id); }
void DX11Renderer::SetCamera(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj) 
{
	XMStoreFloat4x4(&m_view, XMMatrixTranspose(XMLoadFloat4x4(&view)));
	XMStoreFloat4x4(&m_proj, XMMatrixTranspose(XMLoadFloat4x4(&proj)));
}
void DX11Renderer::Draw(uint32_t meshId, const DirectX::XMFLOAT4X4& world) 
{
	auto it = m_meshes.find(meshId);
	if (it == m_meshes.end()) return;
	const GpuMesh& g = it->second;
	CBData cb;
	XMStoreFloat4x4(&cb.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
	cb.view = m_view;
	cb.proj = m_proj;
	m_context->UpdateSubresource(m_cb.Get(), 0, nullptr, &cb, 0, 0);

	UINT stride = sizeof(Vertex), offset = 0;
	m_context->IASetVertexBuffers(0, 1, g.vb.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(g.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_context->DrawIndexed(g.indexCount, 0, 0);
}
