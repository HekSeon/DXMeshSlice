#pragma once
#include <iostream>
#include <iomanip>
#include <Windows.h>

inline void LogInfo(const char* msg)
{
	std::cout << "[INFO] " << msg << "\n";
}

inline void LogError(const char* where, HRESULT hr)
{
	std::cout << "[ERROR] " << where
		<< " | HRESULT = 0x" << std::hex << std::uppercase
		<< std::setw(0) << std::setfill('0') << (unsigned long)hr
		<< std::dec << std::setfill(' ') << "\n";
}

inline void ClearConsole()
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(h, &csbi))return;

	DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
	DWORD written;
	COORD origin{ 0,0 };

	FillConsoleOutputCharacter(h, L' ', cells, origin, &written);
	FillConsoleOutputAttribute(h, csbi.wAttributes, cells, origin, &written);
	SetConsoleCursorPosition(h, origin);
}