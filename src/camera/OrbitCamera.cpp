#include "OrbitCamera.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

OrbitCamera::OrbitCamera()
{
	BuildView();
}

void OrbitCamera::Orbit(int dx, int dy)
{
	m_yaw -= dx * 0.01f;
	m_pitch += dy * 0.01f;
	const float limit = XM_PIDIV2 - 0.05f;
	m_pitch = std::clamp(m_pitch, -limit, limit);
	BuildView();
}

void OrbitCamera::Zoom(float steps)
{
	m_radius = std::clamp(m_radius * expf(-steps * 0.12f), kMinRadius, kMaxRadius);
	BuildView();
}

void OrbitCamera::BuildView()
{
	float cp = cosf(m_pitch);
	XMVECTOR eye = XMVectorSet(m_radius * cp * sinf(m_yaw),
		m_radius * sinf(m_pitch), -m_radius * cp * cosf(m_yaw), 1.0f);
	XMStoreFloat4x4(&m_view,
		XMMatrixLookAtLH(eye, XMVectorZero(), XMVectorSet(0, 1, 0, 0)));
}

XMFLOAT4X4 OrbitCamera::Projection(uint32_t w, uint32_t h) const
{
	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, XMMatrixPerspectiveFovLH(XM_PIDIV4, float(w) / float(h), 0.1f, 100.0f));
	return p;
}

bool OrbitCamera::ScreenLinePlane(int x0, int y0, int x1, int y1,
	uint32_t w, uint32_t h, Plane& out) const
{
	if (w == 0 || h == 0)return false;
	const float tanHalf = tanf(XM_PIDIV4 * 0.5f);
	const float aspect = float(w) / float(h);

	XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_view));
	XMVECTOR camPos = invView.r[3];

	auto rayDir = [&](int px, int py)
		{
			float nx = 2.0f * px / float(w) - 1.0f;
			float ny = 1.0f - 2.0f * py / float(h);
			XMVECTOR d = XMVectorSet(nx * tanHalf * aspect, ny * tanHalf, 1.0f, 0.0f);
			return XMVector3Normalize(XMVector3TransformNormal(d, invView));
		};

	XMVECTOR n = XMVector3Cross(rayDir(x0, y0), rayDir(x1, y1));
	if (XMVectorGetX(XMVector3Length(n)) < 1e-4f)return false;

	n = XMVector3Normalize(n);
	XMStoreFloat3(&out.normal, n);
	out.d = XMVectorGetX(XMVector3Dot(n, camPos));
	return true;
}

