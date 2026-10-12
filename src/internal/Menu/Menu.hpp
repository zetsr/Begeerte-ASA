#pragma once
#include "../../external/Shadow-Gui/src/Shadow.h"
#include "../Config/Config.hpp"

namespace Menu {
	inline void Run() {
		if (!Config::bShowMenu) return;

		if (Shadow::Begin("Begeerte for ASA"))
		{
			if (Shadow::BeginTabBar("MainTabBar", Shadow::ShadowTabBarFlags_NoScrollbar))
			{
				if (Shadow::BeginTabItem("Visuals"))
				{
					Shadow::Text("Visuals Settings");
				}
				Shadow::EndTabItem();

				if (Shadow::BeginTabItem("Misc"))
				{
					Shadow::Text("Misc Settings");
				}
				Shadow::EndTabItem();

			} // EndTabBar
			Shadow::EndTabBar();

		} // End
		Shadow::End();
	}
}