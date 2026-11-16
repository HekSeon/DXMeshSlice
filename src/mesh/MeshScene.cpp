#include "MeshScene.h"
#include "MeshSlicer.h"
#include "SphereGenerator.h"
#include "log.h"
#include <algorithm>
#include <chrono>
#include <cmath>

using namespace DirectX;

void MeshScene::AddPiece(Mesh&& m, XMFLOAT3 offset, double* resourceMs)
{
	const uint32_t id = m_nextId++;
	auto uploadStart = std::chrono::steady_clock::now();
	m_renderer.UploadMesh(id, m);
	if (resourceMs)
		*resourceMs += std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - uploadStart).count();
	XMFLOAT3 lo = m.vertices.front().position, hi = lo;
	for (const Vertex& v : m.vertices)
	{
		lo.x = (std::min)(lo.x, v.position.x);hi.x = (std::max)(hi.x, v.position.x);
		lo.y = (std::min)(lo.y, v.position.y);hi.y = (std::max)(hi.y, v.position.y);
		lo.z = (std::min)(lo.z, v.position.z);hi.z = (std::max)(hi.z, v.position.z);
	}
	XMFLOAT3 center((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f);
	XMFLOAT3 extent((hi.x - lo.x) * 0.5f, (hi.y - lo.y) * 0.5f, (hi.z - lo.z) * 0.5f);
	m_pieces.push_back({ std::move(m),id,offset,center,extent });
}

void MeshScene::Reset()
{
	double resourceMs = 0.0;
	m_lastCutMs = 0.0;
	m_lastSliceMs = 0.0;
	for (const Piece& p : m_pieces)
	{
		auto removeStart = std::chrono::steady_clock::now();
		m_renderer.RemoveMesh(p.id);
		resourceMs += std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - removeStart).count();
	}
	m_pieces.clear();
	AddPiece(GenerateUVSphere(1.0f, 64, 128), XMFLOAT3(0, 0, 0), &resourceMs);
	m_lastMeshUpdateMs = resourceMs;
}

void MeshScene::Slice(const Plane& pl)
{
	auto t0 = std::chrono::steady_clock::now();
	double sliceMs = 0.0, uploadMs = 0.0;
	std::vector<Piece> old = std::move(m_pieces);
	m_pieces.clear();
	m_pieces.reserve(old.size() * 2);

	const float gap = 0.1f;
	bool any = false;
	for (Piece& p : old)
	{
		Plane local = pl;
		local.d = pl.d - (pl.normal.x * p.offset.x +
			pl.normal.y * p.offset.y +
			pl.normal.z * p.offset.z);
		float distance = SignedDistance(local, p.center);
		float reach = fabsf(local.normal.x) * p.extent.x +
			fabsf(local.normal.y) * p.extent.y + fabsf(local.normal.z) * p.extent.z;
		if (fabsf(distance) > reach + 1e-5f)
		{
			m_pieces.push_back(std::move(p));
			continue;
		}
		Mesh a, b;
		auto sliceStart = std::chrono::steady_clock::now();
		bool cut = SliceMesh(p.mesh, local, a, b);
		sliceMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceStart).count();
		if (cut)
		{
			auto removeStart = std::chrono::steady_clock::now();
			m_renderer.RemoveMesh(p.id);
			uploadMs += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - removeStart).count();
			AddPiece(std::move(a), XMFLOAT3(p.offset.x + pl.normal.x * gap,
				p.offset.y + pl.normal.y * gap,
				p.offset.z + pl.normal.z * gap), &uploadMs);
			AddPiece(std::move(b), XMFLOAT3(p.offset.x - pl.normal.x * gap,
				p.offset.y - pl.normal.y * gap,
				p.offset.z - pl.normal.z * gap), &uploadMs);
			any = true;
		}
		else
		{
			m_pieces.push_back(std::move(p));
		}
	}
	old.clear();
	auto t1 = std::chrono::steady_clock::now();
	m_lastCutMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
	m_lastSliceMs = sliceMs;
	m_lastMeshUpdateMs = uploadMs;
	std::cout << "cut: " << m_lastCutMs
		<< " ms, slice: " << m_lastSliceMs
		<< " ms, mesh update: " << m_lastMeshUpdateMs
		<< " ms, pieces: " << m_pieces.size() << "\n";
	if(!any) LogInfo("Line cannot cut the sphere");
}

void MeshScene::Draw()
{
	for (const Piece& p : m_pieces)
	{
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world, XMMatrixTranslation(p.offset.x, p.offset.y, p.offset.z));
		m_renderer.Draw(p.id, world);
	}
}
