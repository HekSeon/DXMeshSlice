#include <iostream>
#include <DirectXMath.h>
#include "RendererFactory.h"
#include "window.h"
#include "SphereGenerator.h"
#include "log.h"
#include "MeshSlicer.h"
#include <vector>
#include <chrono>

struct Piece
{
	Mesh mesh;
	uint32_t id;
	XMFLOAT3 offset;
};


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

static XMFLOAT4X4 MakeProj(uint32_t w, uint32_t h)
{
	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, XMMatrixPerspectiveFovLH(XM_PIDIV4, float(w) / float(h), 0.1f, 100.0f));
	return p;
}

static void WaitForKey()
{
	std::cout << "\nPress Enter...";
	std::cin.clear();
	std::cin.ignore(10000, '\n');
	std::cin.get();
}

static bool PlaneFrameScreenLine(int x0, int y0, int x1, int y1,
	uint32_t w, uint32_t h, const XMFLOAT4X4& view, Plane& out)
{
	const float tanHalf = tanf(XM_PIDIV4 * 0.5f);
	const float aspect = float(w) / float(h);

	XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
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

int main()
{
	GraphicAPI api = AskAPI();


	Window window;
	if (!window.Create(L"DXMeshSlice", 1280, 720)) 
	{
		LogError("Window::Create", HRESULT_FROM_WIN32(GetLastError()));
		WaitForKey();
		return -1;
	}
	LogInfo("Window Created");

	auto renderer = CreateRenderer(api);
	if (!renderer || !renderer->Initialize(window.GetHandle(), window.Width(), window.Height()))
	{
		LogInfo("Renderer not started");
		WaitForKey();
		return -1;
	}
	LogInfo("Renderer ready");

	std::vector<Piece> pieces;
	uint32_t nextid = 0;

	auto addPiece = [&](Mesh&& m, XMFLOAT3 offset)
		{
			const uint32_t id = nextid++;
			renderer->UploadMesh(id, m);
			pieces.push_back({ std::move(m),id,offset });
		};

	addPiece(GenerateUVSphere(1.0f, 64, 128), XMFLOAT3(0, 0, 0));

	XMFLOAT4X4 view, world;
	XMStoreFloat4x4(&view,
		XMMatrixLookAtLH(XMVectorSet(0, 0, -4, 1),
			XMVectorSet(0, 0, 0, 1),
			XMVectorSet(0, 1, 0, 0)));
	XMStoreFloat4x4(&world, XMMatrixIdentity());

	renderer->SetCamera(view, MakeProj(window.Width(), window.Height()));

	LogInfo("Loop Start");

	while (window.PumpMessages())
	{
		uint32_t w = 0, h = 0;
		if (window.ConsumeResize(w, h)&&w>0&&h>0)
		{
			renderer->Resize(w, h);
			renderer->SetCamera(view, MakeProj(w, h));
		}
		int x0, y0, x1, y1;
		if (window.ConsumeLine(x0, y0, x1, y1))
		{
			std::cout << "line: (" << x0 << "," << y0 << ")->(" << x1 << "," << y1 << ")\n";
			Plane pl;
			if (abs(x1 - x0) + abs(y1 - y0) < 5)
			{

			}
			else if (!PlaneFrameScreenLine(x0, y0, x1, y1, window.Width(), window.Height(), view, pl))
			{
				LogInfo("Line gecersiz");
			}
			else
			{
				auto t0 = std::chrono::high_resolution_clock::now();
				std::vector<Piece> old = std::move(pieces);
				pieces.clear();

				const float gap = 0.1f;
				bool any = false;
				for (Piece& p : old)
				{
					Plane local = pl;
					local.d = pl.d - (pl.normal.x * p.offset.x +
						pl.normal.y * p.offset.y +
						pl.normal.z * p.offset.z);
					Mesh a, b;
					if (SliceMesh(p.mesh, local, a, b))
					{
						renderer->RemoveMesh(p.id);
						addPiece(std::move(a), XMFLOAT3(p.offset.x + pl.normal.x * gap,
							p.offset.y + pl.normal.y * gap,
							p.offset.z + pl.normal.z * gap));
						addPiece(std::move(b), XMFLOAT3(p.offset.x - pl.normal.x * gap,
							p.offset.y - pl.normal.y * gap,
							p.offset.z - pl.normal.z * gap));
				
					any = true;
					}
					else
					{
						pieces.push_back(std::move(p));
					}
					auto t1 = std::chrono::high_resolution_clock::now();
					std::cout << "cut: "
						<< std::chrono::duration<double, std::milli>(t1 - t0).count()
						<< " ms, piece count: " << pieces.size() << "\n";
				}
					if(!any) LogInfo("Line cannot cut the SPHIRE");
			}
			
		}

		if (GetAsyncKeyState('R') & 1)
		{
			GetForegroundWindow() == window.GetHandle();
			for (Piece& p : pieces)renderer->RemoveMesh(p.id);
			pieces.clear();
			addPiece(GenerateUVSphere(1.0f, 64, 128), XMFLOAT3(0, 0, 0));
		}
		renderer->BeginFrame();
		for (const Piece& p : pieces)
		{
			XMFLOAT4X4 wm;
			XMStoreFloat4x4(&wm, XMMatrixTranslation(p.offset.x, p.offset.y, p.offset.z));
			renderer->Draw(p.id, wm);
		}
		renderer->EndFrame();
	}

	renderer->Shutdown();
	LogInfo("Shutdown");
	WaitForKey();
	return 0;
}

