/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#include "stdafx.hpp"
#include "alderchess.hpp"

AlderChess::EventManager::EventManager() {
	if (!Renderer.good()) {
		ShowError(Renderer.GetErrorMessage());
		return;
	}
		
	Good = true;
}

bool AlderChess::EventManager::good() {
	return Good;
}

void AlderChess::EventManager::Run() {
	bool QuitApplication = false;
	while (!QuitApplication) {
		Renderer.Render();

		// Wait indefinitely for a new event
		SDL_WaitEvent(&SDL.Event);

		// Consume the event
		SDL_PollEvent(&SDL.Event);

		switch (SDL.Event.type) {
			case SDL_MOUSEMOTION:
				Renderer.HandleMouseHover({SDL.Event.motion.x, SDL.Event.motion.y});
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (SDL.Event.button.button == SDL_BUTTON_LEFT) {
					Renderer.HandleMouseClick({SDL.Event.button.x, SDL.Event.button.y});
				} else if (SDL.Event.button.button == SDL_BUTTON_RIGHT) {
					#ifdef _DEBUG
						Renderer.HandleMouseClickRight({SDL.Event.button.x, SDL.Event.button.y});
					#endif
				}
				break;
			case SDL_QUIT:
				QuitApplication = true;
				break;
		}

		if (Renderer.WantsToQuit()) QuitApplication = true;
	}

	return;
}