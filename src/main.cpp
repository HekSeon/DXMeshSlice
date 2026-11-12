#include <iostream>
#include <DirectXMath.h>
#include "RendererFactory.h"
#include "window.h"
#include "SphereGenerator.h"

GraphicAPI AskAPI()
{
	while (true)
	{
		std::cout << "Graphic API selector:\n 1 - DirectX 11\n 2 - DirectX 12\n";
		int c = 0;
		std::cin >> c;
		if (c == 1)return GraphicAPI::DX11;
		if (c == 2)return GraphicAPI::DX12;
		std::cin.clear();
		std::cin.ignore(10000, '\n');
		std::cout << "Error.\n";
	}
}

static XMFLOAT4X4 MakeProj(uint32_t w, uint32_t h)
{
	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, XMMatrixPerspectiveFovLH(XM_PIDIV4, float(w) / float(h), 0.1f, 100.0f));
	return p;
}

int main()
{
	GraphicAPI api = AskAPI();

	Window window;
	if (!window.Create(L"DXMeshSlice", 1280, 720)) return -1;

	auto renderer = CreateRenderer(api);
	if (!renderer || !renderer->Initialize(window.GetHandle(), window.Width(), window.Height()))
	{
		std::cout << "Renderer not started.\n";
		return -1;
	}

	const uint32_t SPHERE_ID = 0;
	renderer->UploadMesh(SPHERE_ID, GenerateUVSphere(1.0f, 64, 128));

	XMFLOAT4X4 view, world;
	XMStoreFloat4x4(&view,
		XMMatrixLookAtLH(XMVectorSet(0, 0, -4, 1),
			XMVectorSet(0, 0, 0, 1),
			XMVectorSet(0, 1, 0, 0)));
	XMStoreFloat4x4(&world, XMMatrixIdentity());

	renderer->SetCamera(view, MakeProj(window.Width(), window.Height()));

	while (window.PumpMessages())
	{
		uint32_t w, h;
		if (window.ConsumeResize(w, h))renderer->Resize(w, h);

		renderer->BeginFrame();
		renderer->Draw(SPHERE_ID, world);
		renderer->EndFrame();
	}

	renderer->Shutdown();
	return 0;
}
