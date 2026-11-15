#include "MeshSlicer.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

static const float kEps = 1e-5f;

static Vertex Lerp(const Vertex& a, const Vertex& b, float t)
{
	XMVECTOR p = XMVectorLerp(XMLoadFloat3(&a.position), XMLoadFloat3(&b.position), t);
	XMVECTOR n = XMVector3Normalize(XMVectorLerp(XMLoadFloat3(&a.normal), XMLoadFloat3(&b.normal), t));
	Vertex v;
	XMStoreFloat3(&v.position, p);
	XMStoreFloat3(&v.normal, n);
	return v;
}

static void ADDFan(Mesh& m, const std::vector<Vertex>& poly)
{
	if (poly.size() < 3)return;
	const uint32_t base = (uint32_t)m.vertices.size();
	m.vertices.insert(m.vertices.end(), poly.begin(), poly.end());
	for (uint32_t k = 1;k + 1 < poly.size();++k)
		m.indices.insert(m.indices.end(), { base,base + k,base + k + 1 });
}

static void ADDCap(Mesh& m, const std::vector<XMFLOAT3>& rawPts, const XMFLOAT3& planeN, const XMFLOAT3& capNormal)
{
	std::vector<XMVECTOR> pts;
	for (const auto& r : rawPts)
	{
		XMVECTOR p = XMLoadFloat3(&r);
		bool dup = false;
		for (const auto& q : pts)
			if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(p, q))) < 1e-8f) { dup = true; }
		if (!dup)pts.push_back(p);
	}
	if (pts.size() < 3)return;

	//<==	middle	==>
	XMVECTOR c = XMVectorZero();
	for (const auto& p : pts)c = XMVectorAdd(c, p);
	c = XMVectorScale(c, 1.0f / (float)pts.size());

	XMVECTOR n = XMLoadFloat3(&planeN);
	XMVECTOR axis = (fabsf(planeN.x) < 0.9f) ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
	XMVECTOR u = XMVector3Normalize(XMVector3Cross(n, axis));
	XMVECTOR w = XMVector3Cross(n, u);

	auto angle = [&](XMVECTOR p)
		{
			XMVECTOR d = XMVectorSubtract(p, c);
			return atan2f(XMVectorGetX(XMVector3Dot(d, w)), XMVectorGetX(XMVector3Dot(d, u)));
		};
	std::sort(pts.begin(), pts.end(), [&](XMVECTOR a, XMVECTOR b) {return angle(a) < angle(b);});

	XMVECTOR cn = XMLoadFloat3(&capNormal);
	const  uint32_t cnt = (uint32_t)pts.size();
	for (uint32_t k = 0;k < cnt;++k)
	{
		XMVECTOR p0 = c, p1 = pts[k], p2 = pts[(k + 1) % cnt];
		XMVECTOR g = XMVector3Cross(XMVectorSubtract(p1, p0), XMVectorSubtract(p2, p0));
		if (XMVectorGetX(XMVector3Dot(g, cn)) < 0.0f)std::swap(p1, p2);

		const uint32_t base = (uint32_t)m.vertices.size();
		for (XMVECTOR p : {p0, p1, p2})
		{
			Vertex v;
			XMStoreFloat3(&v.position, p);
			v.normal = capNormal;
			m.vertices.push_back(v);
		}
		m.indices.insert(m.indices.end(), { base,base + 1,base + 2 });
	}
}

bool SliceMesh(const Mesh& src, const Plane& plane, Mesh& outPos, Mesh& outNeg)
{
	Mesh pos, neg;
	std::vector<XMFLOAT3> cutPts;

	for (size_t i = 0;i + 2 < src.indices.size();i += 3)
	{
		const Vertex v[3] = { src.vertices[src.indices[i]],
							src.vertices[src.indices[i + 1]],
							src.vertices[src.indices[i + 2]] };

		float dist[3];
		for (int k = 0;k < 3; k++)
		{
			dist[k] = SignedDistance(plane, v[k].position);
			if (fabsf(dist[k]) < kEps)dist[k] = 0.0f;
		}

		std::vector<Vertex>posPoly, negPoly;
		for (int e = 0;e < 3;++e)
		{
			const Vertex& a=v[e];
			const Vertex& b = v[(e + 1) % 3];
			const float da = dist[e], db = dist[(e + 1) % 3];

			if (da >= 0.0f) posPoly.push_back(a);
			if (da <= 0.0f)negPoly.push_back(a);
			if (da == 0.0f)cutPts.push_back(a.position);

			if ((da > 0.0f && db < 0.0f) || (da < 0.0f && db>0.0f))
			{
				Vertex m = Lerp(a, b, da / (da - db));
				posPoly.push_back(m);
				negPoly.push_back(m);
				cutPts.push_back(m.position);
			}
		}
		ADDFan(pos, posPoly);
		ADDFan(neg, negPoly);
	}

	if (pos.vertices.empty() || neg.vertices.empty())return false;

	const XMFLOAT3 n = plane.normal;
	ADDCap(pos, cutPts, n, XMFLOAT3(-n.x, -n.y, -n.z));
	ADDCap(neg, cutPts, n, n);

	outPos = std::move(pos);
	outNeg = std::move(neg);
	return true;
}

