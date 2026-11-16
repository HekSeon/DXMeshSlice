#include "Engine.h"
#include "log.h"
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <iostream>

Engine::~Engine()
{
	m_scene.reset();
	if (m_renderer)
	{
		m_renderer->Shutdown();
		LogInfo("Shutdown");
	}
}

bool Engine::Initialize(GraphicAPI api)
{
	if (!m_window.Create(L"DXMeshSlice", 1280, 720))
	{
		LogError("Window::Create", HRESULT_FROM_WIN32(GetLastError()));
		return false;
	}
	LogInfo("Window Created");
	m_renderer = CreateRenderer(api);
	if (!m_renderer || !m_renderer->Initialize(m_window.GetHandle(), m_window.Width(), m_window.Height()))
	{
		LogInfo("Renderer not started");
		return false;
	}
	LogInfo("Renderer ready");
#ifdef _DEBUG
	LogInfo("Build: Debug (use Release for performance measurements)");
#else
	LogInfo("Build: Release");
#endif
	m_scene = std::make_unique<MeshScene>(*m_renderer);
	m_scene->Reset();
	UpdateCamera();
	if (!m_overlay.Create(m_window.GetHandle()))
	{
		LogError("StatsOverlay::Create", HRESULT_FROM_WIN32(GetLastError()));
		return false;
	}

	m_overlay.SetText(L"Waiting for first frame...");
	return true;
}

int Engine::Run(GraphicAPI api)
{
	if (!Initialize(api))return -1;

	LogInfo("Loop Start");
	LogInfo("VSync enabled: FPS limited to the display refresh rate");
	m_fpsSampleStart = std::chrono::steady_clock::now();

	while (true)
	{
		auto start = std::chrono::steady_clock::now();

		if (!m_window.PumpMessages())break;
		if (IsIconic(m_window.GetHandle()))
		{
			m_fps = 0.0;
			m_fpsFrames = 0;
			WaitMessage();
			m_fpsSampleStart = std::chrono::steady_clock::now();
			continue;
		}

		Update();
		Render();
		auto now = std::chrono::steady_clock::now();
		++m_fpsFrames;
		double seconds = std::chrono::duration<double>(now - m_fpsSampleStart).count();
		if (seconds >= 0.5)
		{
			m_fps = double(m_fpsFrames) / seconds;
			m_fpsFrames = 0;
			m_fpsSampleStart = now;
		}

		m_cpuFrameMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - start).count();

		UpdateOverlay();
	}

	LogInfo("Loop End");
	return 0;
}

void Engine::UpdateCamera()
{
	m_renderer->SetCamera(m_camera.View(), m_camera.Projection(m_window.Width(), m_window.Height()));
}

void Engine::UpdateOverlay()
{
	auto now = std::chrono::steady_clock::now();

	// Olcum her karede; metin yenileme saniyede 10 kez.
	if (now - m_overlayUpdate < std::chrono::milliseconds(100))
		return;

	m_overlayUpdate = now;

	std::wostringstream text;
	text << std::fixed << std::setprecision(2);
	text << L"FPS (VSync): " << std::setprecision(1) << m_fps << L"\n";
	text << std::setprecision(2);

	text << L"Frame wall* (wait incl.): " << m_cpuFrameMs << L" ms\n";

	double gpuMs = m_renderer->GpuMilliseconds();

	if (gpuMs >= 0.0)
		text << L"GPU draw   : " << gpuMs << L" ms\n";
	else
		text << L"GPU draw   : --\n";

	text << L"Last cut   : " << m_scene->LastCutMilliseconds() << L" ms\n"
		<< L"  Slice    : " << m_scene->LastSliceMilliseconds() << L" ms\n"
		<< L"Mesh update: " << m_scene->LastMeshUpdateMilliseconds() << L" ms\n"
		<< L"Pieces     : " << m_scene->PieceCount();

	m_overlay.SetText(text.str());
}

void Engine::Update()
{
	uint32_t w = 0, h = 0;
	if (m_window.ConsumeResize(w, h) && w > 0 && h > 0)
	{
		m_renderer->Resize(w, h);
		UpdateCamera();
	}
	int dx, dy;
	if (m_window.ConsumeOrbit(dx, dy))
	{
		m_camera.Orbit(dx, dy);
		UpdateCamera();
	}
	float zoomSteps;
	if (m_window.ConsumeZoom(zoomSteps))
	{
		m_camera.Zoom(zoomSteps);
		UpdateCamera();
	}
	int x0, y0, x1, y1;
	if (m_window.ConsumeLine(x0, y0, x1, y1) && abs(x1 - x0) + abs(y1 - y0) >= 5)
	{
		std::cout << "line: (" << x0 << "," << y0 << ")->(" << x1 << "," << y1 << ")\n";
		Plane plane;
		if (m_camera.ScreenLinePlane(x0, y0, x1, y1, m_window.Width(), m_window.Height(), plane))
			m_scene->Slice(plane);
		else LogInfo("Line gecersiz");
	}
	if ((GetAsyncKeyState('R') & 1) && GetForegroundWindow() == m_window.GetHandle())
		m_scene->Reset();
}

void Engine::Render()
{
	m_renderer->BeginFrame();
	m_scene->Draw();
	m_renderer->EndFrame();
}
