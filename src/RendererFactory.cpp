//RendererFactory.cpp
#include "RendererFactory.h"
#include "DX11/dx11renderer.h"
#include "DX12/dx12renderer.h"

std::unique_ptr<IRenderer>CreateRenderer(GraphicAPI api)
{
	switch (api)
	{
	case GraphicAPI::DX11: return std::make_unique<DirectX11Renderer>();
	case GraphicAPI::DX12: return std::make_unique<DX12Renderer>();
	}
	return nullptr;
}