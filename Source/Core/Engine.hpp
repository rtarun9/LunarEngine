#pragma once

namespace lunar::core
{
	class Engine
	{
	public:
		Engine(const Config& config);

		void Update(const float delta_time);
		void Render();
	};
}


