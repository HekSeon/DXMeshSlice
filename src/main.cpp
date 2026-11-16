#include "engine/Engine.h"
#include "log.h"
#include <iostream>

GraphicAPI AskAPI()
{
	while (true)
	{
		std::cout << "Graphic API selector:\n 1 - DirectX 11\n 2 - DirectX 12\n";
		int c = 0;
		std::cin >> c;
		if (c == 1) { ClearConsole(); return GraphicAPI::DX11; }
		if (c == 2) { ClearConsole(); return GraphicAPI::DX12; }
		std::cin.clear();
		std::cin.ignore(10000, '\n');
		std::cout << "Error.\n";
	}
}

static void WaitForKey()
{
	std::cout << "\nPress Enter...";
	std::cin.clear();
	std::cin.ignore(10000, '\n');
	std::cin.get();
}

int main()
{
	int result = 0;
	{
		Engine engine;
		result = engine.Run(AskAPI());
	}
	WaitForKey();
	return result;
}
