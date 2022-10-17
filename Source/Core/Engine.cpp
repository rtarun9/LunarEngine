#include "Pch.hpp"

#include "Engine.hpp"

namespace lunar::core
{
	Engine::Engine(const Config& config)
	{
		m_graphics_device = std::make_unique<gfx::GraphicsDevice>();
	}

	void Engine::Update(const float delta_time)
	{
	}

	void Engine::Render()
	{
		m_graphics_device->GetDirectCommandQueue()->Flush();
	}
}
