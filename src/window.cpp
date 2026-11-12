#include "window.h"

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
	m_hwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW,
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
		if (msg.message = WM_QUIT)return false;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

bool Window::ConsumeResize(uint32_t& w, uint32_t h)
{
	if (!m_resized)return false;
	m_resized = false;
	w = m_width; h = m_height;
	return true;
}

LRESULT Window::WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
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
	case WM_SIZE:
		if (wp != SIZE_MINIMIZED)
		{
			m_width = LOWORD(lp);
			m_height = LOWORD(lp);
			m_resized = true;
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(h, m, wp, lp);
}


