//RendererFactory.h
#pragma once
#include <memory>
#include "IRenderer.h"

enum class GraphicAPI {DX11=1,DX12=2};

std::unique_ptr<IRenderer>CreateRenderer(GraphicAPI api);