#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <unordered_map>
#include <DirectXMath.h>
#include "../IRenderer.h"



class DirectX11Renderer : public IRenderer
{
public:
	bool Initialize(HWND hwnd, uint32_t width, uint32_t height) override;
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
	struct GpuMesh
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>vb;
		Microsoft::WRL::ComPtr<ID3D11Buffer>ib;
		UINT indexCount = 0;
	};

	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain>		m_swapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
	std::unordered_map<uint32_t, GpuMesh> m_meshes;

	void CreateRendererTarget();
	uint32_t m_width = 0, m_height = 0;

};