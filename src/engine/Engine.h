#pragma once
#include "RendererFactory.h"
#include "window/window.h"
#include "camera/OrbitCamera.h"
#include "Mesh/MeshScene.h"
#include "statsoverlay.h"
#include <chrono>
#include <memory>

class Engine
{
public:
	~Engine();
	int Run(GraphicAPI api);

private:
	bool Initialize(GraphicAPI api);
	void Update();
	void Render();
	void UpdateCamera();
	void UpdateOverlay();

	StatsOverlay m_overlay;
	double m_cpuFrameMs = 0.0;
	double m_fps = 0.0;
	uint32_t m_fpsFrames = 0;
	std::chrono::steady_clock::time_point m_fpsSampleStart{};

	std::chrono::steady_clock::time_point m_overlayUpdate{};

	Window m_window;
	OrbitCamera m_camera;
	std::unique_ptr<IRenderer> m_renderer;
	std::unique_ptr<MeshScene> m_scene;
};
