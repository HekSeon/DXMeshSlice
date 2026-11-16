#include "MeshSlicer.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

using namespace DirectX;

static const float kEps = 1e-5f;

struct Cell
{
	int64_t x, y, z;
	bool operator==(const Cell&) const = default;
};

struct CellHash
{
	size_t operator()(const Cell& c) const
	{
		size_t h = std::hash<int64_t>{}(c.x);
		h ^= std::hash<int64_t>{}(c.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h ^ (std::hash<int64_t>{}(c.z) + 0x9e3779b9 + (h << 6) + (h >> 2));
	}
};

static Vertex Lerp(const Vertex& a, const Vertex& b, float t)
{
	XMVECTOR p = XMVectorLerp(XMLoadFloat3(&a.position), XMLoadFloat3(&b.position), t);
	XMVECTOR n = XMVector3Normalize(XMVectorLerp(XMLoadFloat3(&a.normal), XMLoadFloat3(&b.normal), t));
	Vertex v;
	XMStoreFloat3(&v.position, p);
	XMStoreFloat3(&v.normal, n);
	return v;
}

static uint32_t ADDVertex(Mesh& mesh, const Mesh& src, uint32_t index, std::vector<uint32_t>& remap)
{
	if (remap[index] == UINT32_MAX)
	{
		remap[index] = (uint32_t)mesh.vertices.size();
		mesh.vertices.push_back(src.vertices[index]);
	}
	return remap[index];
}

static void ADDFan(Mesh& mesh, const uint32_t* polygon, uint32_t count)
{
	for (uint32_t k = 1;k + 1 < count;++k)
		mesh.indices.insert(mesh.indices.end(), { polygon[0],polygon[k],polygon[k + 1] });
}

static std::vector<XMVECTOR> PrepareCap(const std::vector<XMFLOAT3>& rawPts, const XMFLOAT3& planeN)
{
	std::vector<XMVECTOR> pts;
	pts.reserve(rawPts.size());
	std::unordered_map<Cell, std::vector<size_t>, CellHash> cells;
	cells.reserve(rawPts.size());
	for (const auto& r : rawPts)
	{
		XMVECTOR p = XMLoadFloat3(&r);
		Cell cell{ (int64_t)floor(double(r.x) / 1e-4),
			(int64_t)floor(double(r.y) / 1e-4), (int64_t)floor(double(r.z) / 1e-4) };
		bool dup = false;
		for (int x = -1;x <= 1 && !dup;++x)
			for (int y = -1;y <= 1 && !dup;++y)
				for (int z = -1;z <= 1 && !dup;++z)
				{
					auto it = cells.find({ cell.x + x, cell.y + y, cell.z + z });
					if (it == cells.end())continue;
					for (size_t i : it->second)
						if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(p, pts[i]))) < 1e-8f)
						{ dup = true;break; }
				}
		if (!dup)
		{
			cells[cell].push_back(pts.size());
			pts.push_back(p);
		}
	}
	if (pts.size() < 3)return {};

	//<==	middle	==>
	XMVECTOR c = XMVectorZero();
	for (const auto& p : pts)c = XMVectorAdd(c, p);
	c = XMVectorScale(c, 1.0f / (float)pts.size());

	XMVECTOR n = XMLoadFloat3(&planeN);
	XMVECTOR axis = (fabsf(planeN.x) < 0.9f) ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
	XMVECTOR u = XMVector3Normalize(XMVector3Cross(n, axis));
	XMVECTOR w = XMVector3Cross(n, u);

	struct CapPoint
	{
		XMVECTOR position;
		float angle;
	};
	std::vector<CapPoint> sorted;
	sorted.reserve(pts.size());
	for (XMVECTOR p : pts)
	{
		XMVECTOR d = XMVectorSubtract(p, c);
		sorted.push_back({ p,atan2f(XMVectorGetX(XMVector3Dot(d, w)), XMVectorGetX(XMVector3Dot(d, u))) });
	}
	std::sort(sorted.begin(), sorted.end(), [](const CapPoint& a, const CapPoint& b) { return a.angle < b.angle; });
	for (size_t i = 0;i < sorted.size();++i)pts[i] = sorted[i].position;
	return pts;
}

static void ADDCap(Mesh& m, const std::vector<XMVECTOR>& pts, const XMFLOAT3& capNormal)
{
	if (pts.size() < 3)return;
	XMVECTOR c = XMVectorZero();
	for (XMVECTOR p : pts)c = XMVectorAdd(c, p);
	c = XMVectorScale(c, 1.0f / float(pts.size()));
	XMVECTOR cn = XMLoadFloat3(&capNormal);
	const  uint32_t cnt = (uint32_t)pts.size();
	const uint32_t base = (uint32_t)m.vertices.size();
	Vertex center;
	XMStoreFloat3(&center.position, c);
	center.normal = capNormal;
	m.vertices.push_back(center);
	for (XMVECTOR p : pts)
	{
		Vertex v;
		XMStoreFloat3(&v.position, p);
		v.normal = capNormal;
		m.vertices.push_back(v);
	}
	for (uint32_t k = 0;k < cnt;++k)
	{
		XMVECTOR p0 = c, p1 = pts[k], p2 = pts[(k + 1) % cnt];
		XMVECTOR g = XMVector3Cross(XMVectorSubtract(p1, p0), XMVectorSubtract(p2, p0));
		uint32_t a = base + 1 + k, b = base + 1 + (k + 1) % cnt;
		if (XMVectorGetX(XMVector3Dot(g, cn)) < 0.0f)std::swap(a, b);
		m.indices.insert(m.indices.end(), { base,a,b });
	}
}

bool SliceMesh(const Mesh& src, const Plane& plane, Mesh& outPos, Mesh& outNeg)
{
	// Her vertex icin mesafeyi bir kez hesapla.
	std::vector<float> distances(src.vertices.size());
	bool hasPos = false, hasNeg = false;
	for (size_t i = 0;i < src.vertices.size();++i)
	{
		float d = SignedDistance(plane, src.vertices[i].position);
		if (fabsf(d) < kEps)d = 0.0f;
		distances[i] = d;
	}
	for (uint32_t i : src.indices)
	{
		hasPos |= distances[i] > 0.0f;
		hasNeg |= distances[i] < 0.0f;
		if (hasPos && hasNeg)break;
	}
	if (!hasPos || !hasNeg)return false;

	Mesh pos, neg;
	pos.vertices.reserve(src.vertices.size());neg.vertices.reserve(src.vertices.size());
	pos.indices.reserve(src.indices.size());neg.indices.reserve(src.indices.size());
	std::vector<uint32_t> posRemap(src.vertices.size(), UINT32_MAX);
	std::vector<uint32_t> negRemap(src.vertices.size(), UINT32_MAX);
	std::vector<XMFLOAT3> cutPts;
	// Duzlem uzerindeki ortak vertexleri sadece bir kez ekle.
	for (size_t i = 0;i < src.vertices.size();++i)
		if (distances[i] == 0.0f)cutPts.push_back(src.vertices[i].position);

	struct EdgeVertex { uint32_t pos, neg; };
	std::unordered_map<uint64_t, EdgeVertex> edges;
	for (size_t i = 0;i + 2 < src.indices.size();i += 3)
	{
		uint32_t ids[3] = { src.indices[i],src.indices[i + 1],src.indices[i + 2] };
		uint32_t posPoly[4], negPoly[4];
		uint32_t posCount = 0, negCount = 0;
		for (int e = 0;e < 3;++e)
		{
			uint32_t a = ids[e], b = ids[(e + 1) % 3];
			float da = distances[a], db = distances[b];
			if (da >= 0.0f)posPoly[posCount++] = ADDVertex(pos, src, a, posRemap);
			if (da <= 0.0f)negPoly[negCount++] = ADDVertex(neg, src, a, negRemap);
			if ((da > 0.0f && db < 0.0f) || (da < 0.0f && db > 0.0f))
			{
				uint32_t lo = (std::min)(a, b), hi = (std::max)(a, b);
				uint64_t key = (uint64_t(lo) << 32) | hi;
				auto it = edges.find(key);
				if (it == edges.end())
				{
					Vertex v = Lerp(src.vertices[lo], src.vertices[hi], distances[lo] / (distances[lo] - distances[hi]));
					EdgeVertex index{ (uint32_t)pos.vertices.size(),(uint32_t)neg.vertices.size() };
					pos.vertices.push_back(v);neg.vertices.push_back(v);
					cutPts.push_back(v.position);
					it = edges.emplace(key, index).first;
				}
				posPoly[posCount++] = it->second.pos;
				negPoly[negCount++] = it->second.neg;
			}
		}
		ADDFan(pos, posPoly, posCount);ADDFan(neg, negPoly, negCount);
	}
	if (pos.indices.empty() || neg.indices.empty())return false;

	const XMFLOAT3 n = plane.normal;
	auto cap = PrepareCap(cutPts, n);
	ADDCap(pos, cap, XMFLOAT3(-n.x, -n.y, -n.z));
	ADDCap(neg, cap, n);
	outPos = std::move(pos);outNeg = std::move(neg);
	return true;
}
