#pragma once 
#include <Windows.h>
#include <cstdint>

class Window
{
public:
	bool Create(const wchar_t* title, uint32_t width, uint32_t height);
	bool PumpMessages();
	bool ConsumeResize(uint32_t& w, uint32_t& h);

	HWND GetHandle() const { return m_hwnd; }
	uint32_t Width() const { return m_width; }
	uint32_t Height() const { return m_height; }

private:
	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
	LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

	HWND m_hwnd	= nullptr;
	uint32_t m_width = 0, m_height = 0;
	bool m_resized = false;
};