#pragma once
#include "../IRenderer.h"


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
};