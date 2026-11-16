#pragma once
#include <Windows.h>
#include <string>
#include <algorithm>

class StatsOverlay
{
public:
	~StatsOverlay()
	{
		if (m_hwnd)DestroyWindow(m_hwnd);
		if (m_font)DeleteObject(m_font);
	}

	bool Create(HWND parent)
	{
		HINSTANCE instance = GetModuleHandle(nullptr);
		WNDCLASSW wc{};
		wc.lpfnWndProc = WndProc;
		wc.hInstance = instance;
		wc.lpszClassName = L"DXMeshSliceStats";
		if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			return false;

		m_font = CreateFontW(-22, 0, 0, 0, FW_MEDIUM,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

		m_hwnd = CreateWindowExW(WS_EX_NOACTIVATE, wc.lpszClassName, L"",
			WS_CHILD | WS_VISIBLE, 10, 10, 360, 160,
			parent, nullptr, instance, this);
		return m_hwnd != nullptr;
	}

	void SetText(const std::wstring& text)
	{
		if (!m_hwnd || text == m_text)return;
		m_text = text;
		HDC dc = GetDC(m_hwnd);
		if (dc)
		{
			HGDIOBJ oldFont = SelectObject(dc,
				m_font ? static_cast<HGDIOBJ>(m_font) : GetStockObject(DEFAULT_GUI_FONT));
			RECT measured{};
			DrawTextW(dc, m_text.c_str(), -1, &measured,
				DT_LEFT | DT_TOP | DT_NOPREFIX | DT_CALCRECT);
			SelectObject(dc, oldFont);
			ReleaseDC(m_hwnd, dc);

			RECT client{};
			GetClientRect(m_hwnd, &client);
			int width = (std::max)(360L, measured.right - measured.left + 24);
			int height = measured.bottom - measured.top + 20;
			if (width != client.right || height != client.bottom)
				SetWindowPos(m_hwnd, nullptr, 0, 0, width, height,
					SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		InvalidateRect(m_hwnd, nullptr, FALSE);
	}

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
	{
		auto self = reinterpret_cast<StatsOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
			self = static_cast<StatsOverlay*>(cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}

		switch (message)
		{
		case WM_NCHITTEST:
			return HTTRANSPARENT;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			HDC dc = BeginPaint(hwnd, &ps);
			RECT rect{};
			GetClientRect(hwnd, &rect);
			FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
			HGDIOBJ oldFont = SelectObject(dc,
				self && self->m_font ? static_cast<HGDIOBJ>(self->m_font)
				: GetStockObject(DEFAULT_GUI_FONT));
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, RGB(220, 255, 220));
			InflateRect(&rect, -12, -10);
			if (self)DrawTextW(dc, self->m_text.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);
			SelectObject(dc, oldFont);
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_NCDESTROY:
			if (self)self->m_hwnd = nullptr;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
			break;
		}
		return DefWindowProcW(hwnd, message, wp, lp);
	}

	HWND m_hwnd = nullptr;
	HFONT m_font = nullptr;
	std::wstring m_text;
};
