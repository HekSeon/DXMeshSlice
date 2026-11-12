#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <unordered_map>
#include <DirectXMath.h>
#include "../IRenderer.h"

using Microsoft::WRL::ComPtr;

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
		ComPtr<ID3D11Buffer>			vb;
		ComPtr<ID3D11Buffer>			ib;
		UINT indexCount					= 0;
	};

	struct CBData
	{
		XMFLOAT4X4 world;
		XMFLOAT4X4 view;
		XMFLOAT4X4 proj;
	};

	void CreateRendererTarget();
	void CreateDepthBuffer();
	bool CreateShaders();
	bool CreatePipelineStates();

	ComPtr<ID3D11Device>					m_device;
	ComPtr<ID3D11DeviceContext>				m_context;
	ComPtr<IDXGISwapChain>					m_swapChain;
	ComPtr<ID3D11RenderTargetView>			m_rtv;
	ComPtr<ID3D11Texture2D>					m_depthTex;
	ComPtr<ID3D11DepthStencilView>			m_dsv;
	ComPtr<ID3D11VertexShader>				m_vs;
	ComPtr<ID3D11PixelShader>				m_ps;
	ComPtr<ID3D11InputLayout>				m_layout;
	ComPtr<ID3D11Buffer>					m_cb;
	ComPtr<ID3D11RasterizerState>			m_raster;
	
	std::unordered_map<uint32_t, GpuMesh>	m_meshes;

	XMFLOAT4X4								m_view;
	XMFLOAT4X4								m_proj;

	uint32_t m_width = 0, m_height = 0;

};