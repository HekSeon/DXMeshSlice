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