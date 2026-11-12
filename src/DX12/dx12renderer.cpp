#include "dx12renderer.h"
#include <iostream>

bool DX12Renderer::Initialize(HWND, uint32_t, uint32_t)
{
	std::cout << "work in progress.\n";
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
