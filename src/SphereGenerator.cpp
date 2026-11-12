#include "SphereGenerator.h"
#include <cmath>

using namespace DirectX;

Mesh GenerateUVSphere(float radius, uint32_t stacks, uint32_t slices)
{
	Mesh m;
	m.vertices.reserve((stacks + 1) * (slices + 1));

	for (uint32_t i = 0; i <= stacks;i++)
	{
		float phi = XM_PI * float(i) / float(stacks);
		for (uint32_t j = 0;j <= slices;j++)
		{
			float theta = XM_2PI * float(j) / float(slices);
			XMFLOAT3 n(sinf(phi) * cosf(theta),
				cosf(phi), 
				sinf(phi) * sinf(theta));
			m.vertices.push_back({ { n.x * radius, n.y * radius, n.z * radius },n });
		}
	}

	const uint32_t ring = slices + 1;
	for (uint32_t i = 0;i < stacks;++i)
	{
		for (uint32_t j = 0;j < slices;++j)
		{
			uint32_t a = i * ring + j;
			uint32_t b = a + ring;
			m.indices.insert(m.indices.end(), { a,a + 1,b,a + 1,b + 1,b });
		}
	}

	return m;
}
