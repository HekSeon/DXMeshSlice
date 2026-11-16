#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include "../Plane.h"

class OrbitCamera
{
public:
	OrbitCamera();
	void Orbit(int dx, int dy);
	void Zoom(float steps);
	const DirectX::XMFLOAT4X4& View() const { return m_view; }
	DirectX::XMFLOAT4X4 Projection(uint32_t w, uint32_t h) const;
	bool ScreenLinePlane(int x0, int y0, int x1, int y1,
		uint32_t w, uint32_t h, Plane& out) const;

private:
	void BuildView();
	float m_yaw = 0.0f, m_pitch = 0.0f;
	float m_radius = 4.0f;
	static constexpr float kMinRadius = 1.5f, kMaxRadius = 20.0f;
	DirectX::XMFLOAT4X4 m_view{};
};
