#pragma once
#include "IRenderer.h"
#include "../Plane.h"
#include <vector>

class MeshScene
{
public:
	explicit MeshScene(IRenderer& renderer) : m_renderer(renderer) {}
	void Reset();
	void Slice(const Plane& plane);
	void Draw();

	double LastCutMilliseconds() const { return m_lastCutMs; }
	double LastSliceMilliseconds() const { return m_lastSliceMs; }
	double LastMeshUpdateMilliseconds() const { return m_lastMeshUpdateMs; }
	size_t PieceCount() const { return m_pieces.size(); }

private:
	struct Piece
	{
		Mesh mesh;
		uint32_t id;
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT3 center, extent;
	};

	void AddPiece(Mesh&& mesh, DirectX::XMFLOAT3 offset, double* resourceMs = nullptr);
	IRenderer& m_renderer;
	std::vector<Piece> m_pieces;
	uint32_t m_nextId = 0;
	double m_lastCutMs = 0.0;
	double m_lastSliceMs = 0.0;
	double m_lastMeshUpdateMs = 0.0;
};
