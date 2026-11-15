#pragma once
#include <DirectXMath.h>


struct Plane
{
	DirectX::XMFLOAT3 normal;
	float d;
};

inline float SignedDistance(const Plane& pl, const DirectX::XMFLOAT3& p)
{
	return pl.normal.x * p.x + pl.normal.y * p.y + pl.normal.z * p.z - pl.d;
}