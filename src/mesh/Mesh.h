#pragma once 
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

using namespace DirectX;
struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};