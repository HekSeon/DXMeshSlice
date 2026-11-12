#include "dx11renderer.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#include "../log.h"

using Microsoft::WRL::ComPtr;

bool DirectX11Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
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

	CreateRendererTarget();
	m_width = width, m_height = height;

	return true;
}

void DirectX11Renderer::CreateRendererTarget()//<==========renderer target creating
{
	ComPtr<ID3D11Texture2D> backBuffer;
	m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv);
}

void DirectX11Renderer::Resize(uint32_t width, uint32_t height) //<====Resizing window
{
	if (!m_swapChain || width == 0 || height == 0) return;
	m_context->OMSetRenderTargets(0, nullptr, nullptr);
	m_rtv.Reset();
	m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	CreateRendererTarget();
	m_width = width;m_height = height;
}

void DirectX11Renderer::Shutdown() //<=======Clear
{
	m_meshes.clear();
	m_rtv.Reset();
	m_swapChain.Reset();
	m_context.Reset();
	m_device.Reset();
}

void DirectX11Renderer::BeginFrame() //<=====FrameCount start
{
	const float clear[4] = { 0.08f,0.08f,0.12f,1.0f };
	m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
	m_context->ClearRenderTargetView(m_rtv.Get(), clear);

	D3D11_VIEWPORT vp{ 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
	m_context->RSSetViewports(1, &vp);
}

void DirectX11Renderer::EndFrame() //<=====framecount end
{
	m_swapChain->Present(1, 0);
}

void DirectX11Renderer::UploadMesh(uint32_t, const Mesh&) {}
void DirectX11Renderer::RemoveMesh(uint32_t) {}
void DirectX11Renderer::SetCamera(const DirectX::XMFLOAT4X4&, const DirectX::XMFLOAT4X4&) {}
void DirectX11Renderer::Draw(uint32_t, const DirectX::XMFLOAT4X4&) {}