#pragma once 

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>
#include "Mesh.h"

class IRenderer
{
public:
	virtual ~IRenderer() = default;

	virtual bool Initialize(HWND hwnd, uint32_t width, uint32_t height) = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;
	virtual void Shutdown() = 0;

	virtual void UploadMesh(uint32_t id, const Mesh& mesh) = 0;
	virtual void RemoveMesh(uint32_t id) = 0;

	virtual void SetCamera(const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& proj) = 0;

	virtual void BeginFrame() = 0;
	virtual void Draw(uint32_t meshId, const DirectX::XMFLOAT4X4& world) = 0;
	virtual void EndFrame() = 0;

private:

};