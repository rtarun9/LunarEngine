#pragma once

#include "Graphics/GraphicsDevice.hpp"

namespace lunar::core
{
	class Engine
	{
	public:
		Engine(const Config& config);

		void Update(const float delta_time);
		void Render();

	private:
		std::unique_ptr<gfx::GraphicsDevice> m_graphics_device{};
	};
}


