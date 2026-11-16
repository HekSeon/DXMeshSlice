#include "window.h"
#include <windowsx.h>
#include "log.h"

bool Window::Create(const wchar_t* title, uint32_t width, uint32_t height)
{
	HINSTANCE inst = GetModuleHandle(nullptr);

	WNDCLASSEXW wc{};
	wc.cbSize					= sizeof(wc);
	wc.style					= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc				= WndProc;
	wc.hInstance				= inst;
	wc.hCursor					= LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName			= L"DXMeshSliceWnd";
	RegisterClassExW(&wc);

	RECT rc{ 0,0,(LONG)width,(LONG)height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	m_width = width; m_height = height;
	m_hwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
		rc.bottom - rc.top, nullptr, nullptr, inst, this);
	if (!m_hwnd) return false;

	ShowWindow(m_hwnd, SW_SHOW);

	return true;
}

bool Window::PumpMessages()
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			LogInfo("WM_QUIT running");
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

bool Window::ConsumeResize(uint32_t& w, uint32_t& h)
{
	if (!m_resized)return false;
	m_resized = false;
	w = m_width; h = m_height;
	return true;
}

bool Window::ConsumeLine(int& x0, int& y0, int& x1, int& y1)
{
	if (!m_lineReady)return false;
	m_lineReady = false;
	x0 = m_sx;y0 = m_sy;x1 = m_ex;y1 = m_ey;
	return true;
}

bool Window::ConsumeOrbit(int& dx, int& dy)
{
	if (m_orbitDx == 0 && m_orbitDy == 0)return false;
	dx = m_orbitDx;	dy = m_orbitDy;
	m_orbitDx = m_orbitDy = 0;
	return true;
}

bool Window::ConsumeZoom(float& steps)
{
	if (m_zoomSteps == 0.0f)return false;
	steps = m_zoomSteps;
	m_zoomSteps = 0.0f;
	return true;
}

LRESULT Window::WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
	if (m == WM_CLOSE || m == WM_DESTROY) LogInfo(m == WM_CLOSE ? "WndProc: WM_CLOSE" : "WndProc: WM_DESTROY");
	Window* self = nullptr;
	if (m == WM_NCCREATE)
	{
		auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
		self = static_cast<Window*>(cs->lpCreateParams);
		SetWindowLongPtr(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	else
	{
		self = reinterpret_cast<Window*>(GetWindowLongPtr(h, GWLP_USERDATA));
	}
	return self ? self->HandleMessage(h, m, wp, lp) : DefWindowProc(h, m, wp, lp);
}

LRESULT Window::HandleMessage(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
	switch (m)
	{
	case WM_CLOSE: LogInfo("WM_CLOSE runing"); break;
	case WM_SIZE:
		if (wp != SIZE_MINIMIZED)
		{
			m_width = LOWORD(lp);
			m_height = HIWORD(lp);
			m_resized = true;
		}
		return 0;
	case WM_DESTROY:
		LogInfo("Wm_Destroy runing");
		PostQuitMessage(0);
		return 0;
	case WM_LBUTTONDOWN:
		m_dragging = true;
		m_sx = m_ex = GET_X_LPARAM(lp);
		m_sy = m_ey = GET_Y_LPARAM(lp);
		SetCapture(h);
		return 0;
	case WM_MOUSEMOVE:
		if (m_dragging) { m_ex = GET_X_LPARAM(lp); m_ey = GET_Y_LPARAM(lp); }
		if (m_orbiting)
		{
			int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
			m_orbitDx += x - m_lastX;
			m_orbitDy += y - m_lastY;
			m_lastX = x; m_lastY = y;
		}
		return 0;

	case WM_LBUTTONUP:
		if (m_dragging)
		{
			m_ex = GET_X_LPARAM(lp);m_ey = GET_Y_LPARAM(lp);
			m_dragging = false;
			m_lineReady = true;
			ReleaseCapture();
		}
		return 0;
	case WM_RBUTTONDOWN:
		m_orbiting = true;
		m_lastX = GET_X_LPARAM(lp);
		m_lastY = GET_Y_LPARAM(lp);
		SetCapture(h);
		return 0;
	case WM_MOUSEWHEEL:
		m_zoomSteps += float(GET_WHEEL_DELTA_WPARAM(wp)) / float(WHEEL_DELTA);
		return 0;
	case WM_RBUTTONUP:
		if (m_orbiting) { m_orbiting = false;ReleaseCapture(); }
		return 0;
	case WM_CAPTURECHANGED:
		m_dragging = false;
		m_orbiting = false;
		return 0;
	}
		
	return DefWindowProc(h, m, wp, lp);
}
