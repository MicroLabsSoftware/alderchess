/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#include "stdafx.hpp"
#include "alderchess.hpp"

AlderChess::GameRenderer::GameRenderer() {
	/* In all error conditions we return from constructor
	   which leads to the Good variable staying false. */

	// Initializing the SDL video subsystem also initializes the SDL event subsystem

	SetErrorMessage("Simple DirectMedia Layer failed to initialize.");

	if (SDL_Init(SDL_INIT_VIDEO) != 0) return;

	SDL.Window = SDL_CreateWindow("Alder Chess",
	                              SDL_WINDOWPOS_UNDEFINED,
	                              SDL_WINDOWPOS_UNDEFINED,
	                              256,
	                              256,
	                              SDL_WINDOW_HIDDEN);

	SDL.WindowDisplayIndex = SDL_GetWindowDisplayIndex(SDL.Window);

	if (SDL.WindowDisplayIndex < 0) return;

	float DPIHorizontal = 0;
	int DPIResult = SDL_GetDisplayDPI(SDL.WindowDisplayIndex, NULL, &DPIHorizontal, NULL);

	// If no usable DPI information, then use 100 DPI
	if (DPIResult < 0 || DPIHorizontal < 1.0f) {
		DPIHorizontal = 100.0f;
	}

	/* std::max is used to set a lower limit of 100 DPI.
	   Dividing by 50 leads to lowest possible UI scale of 2.
	   The scale increases by 1 for every additional 50 DPI. */
	UI.Scale = (int)(round(std::max(DPIHorizontal, 100.0f) / 50.0f));

	/* Verify bounds just in case that something crazy happens.
	   Minimum scale 2x, maximum scale 100x. */
	UI.Scale = std::max(2, std::min(UI.Scale, 100));

	UI.WindowSize = ComputeWindowSize();

	SDL_SetWindowSize(SDL.Window, UI.WindowSize.x, UI.WindowSize.y);

	SDL.Renderer = SDL_CreateRenderer(SDL.Window,
	                                  -1,
	                                  SDL_RENDERER_ACCELERATED |
	                                  SDL_RENDERER_PRESENTVSYNC);

	if (SDL.Renderer == NULL) return;

	// 1 = Linear interpolation
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	// Vertical sync
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

	// Translucency
	SDL_SetRenderDrawBlendMode(SDL.Renderer, SDL_BLENDMODE_BLEND);

	SetErrorMessage("Unable to load resources.");

	if (!LoadResources()) return;

	if (!PrepareApplicationIcon()) return;

	SetErrorMessage("The font backend failed to initialize properly.");

	XY LabelSize = { UI.WindowSize.x - 2 * UI.Scale * UI.Metric.WhiteSpace,
	                 UI.Scale * UI.Metric.Text };

	Text = std::make_unique<AlderChess::GameText>
	                       (std::make_pair<std::string, int>
	                            (std::string("caveat.ttf"),
	                                 (int)UI.Metric.FontSizeWestern),
	                        std::make_pair<std::string, int>
	                            (std::string("typo_papyrus_m.ttf"),
	                                 (int)UI.Metric.FontSizeHangul),
	                        LabelSize,
	                        (unsigned int)std::round(DPIHorizontal),
	                        SDL.Renderer);

	if (!Text->good()) return;

	SetErrorMessage("");

	UI.Language = LanguageType::English;

	#ifdef _WIN32
		switch(GetUserDefaultUILanguage() & 0xFF) {
			case 0x12:
				UI.Language = LanguageType::Korean;
				break;
			case 0x0B:
				UI.Language = LanguageType::Finnish;
				break;
		}
	#endif

	Good = true;

	UpdateSpotlightMap();
	UpdateTurnLabel();

	SDL_ShowWindow(SDL.Window);
}

AlderChess::GameRenderer::~GameRenderer() {
	if (!Good) return;

	Text.reset();

	if (SDL.Texture.Select != NULL) SDL_DestroyTexture(SDL.Texture.Select);

	for (SDL_Texture *t : SDL.Texture.Icon) {
		if (t != NULL) SDL_DestroyTexture(t);
	}

	for (SDL_Texture *t : SDL.Texture.BlackPiece) {
		if (t != NULL) SDL_DestroyTexture(t);
	}

	for (SDL_Texture *t : SDL.Texture.WhitePiece) {
		if (t != NULL) SDL_DestroyTexture(t);
	}

	SDL_DestroyRenderer(SDL.Renderer);
	SDL_DestroyWindow(SDL.Window);
	SDL_Quit();
}

bool AlderChess::GameRenderer::good() {
	return Good;
}

std::string AlderChess::GameRenderer::GetErrorMessage() {
	return ErrorMessage;
}

void AlderChess::GameRenderer::SetErrorMessage(std::string ErrorMessageNew) {
	ErrorMessage = ErrorMessageNew;
	return;
}

bool AlderChess::GameRenderer::WantsToQuit() {
	return QuitApplication;
}

bool AlderChess::GameRenderer::LoadResources() {
	std::vector<const char *> Icon = {"i_language.bmp",
	                                  "i_spotlight.bmp",
	                                  "i_restart.bmp",
	                                  "i_load.bmp",
	                                  "i_save.bmp",
	                                  "i_information.bmp",
	                                  "i_quit.bmp"
	};
	
	std::vector<const char*> PieceBlack = {"b_pawn.bmp",
	                                       "b_knight.bmp",
	                                       "b_bishop.bmp",
	                                       "b_rook.bmp",
	                                       "b_dame.bmp",
	                                       "b_king.bmp"
	};

	std::vector<const char*> PieceWhite = {"w_pawn.bmp",
	                                       "w_knight.bmp",
	                                       "w_bishop.bmp",
	                                       "w_rook.bmp",
	                                       "w_dame.bmp",
	                                       "w_king.bmp"
	};

	SDL_Surface *BitmapSurface = NULL;

	BitmapSurface = SDL_LoadBMP("select.bmp");
	if (BitmapSurface == NULL) return false;
	SDL.Texture.Select = SDL_CreateTextureFromSurface(SDL.Renderer, BitmapSurface);

	for (const char *FileName : Icon) {
		BitmapSurface = SDL_LoadBMP(FileName);
		if (BitmapSurface == NULL) return false;
		SDL.Texture.Icon.push_back(SDL_CreateTextureFromSurface(SDL.Renderer, BitmapSurface));
		SDL_FreeSurface(BitmapSurface);
	}

	for (const char *FileName : PieceBlack) {
		BitmapSurface = SDL_LoadBMP(FileName);
		if (BitmapSurface == NULL) return false;
		SDL.Texture.BlackPiece.push_back(SDL_CreateTextureFromSurface(SDL.Renderer, BitmapSurface));
		SDL_FreeSurface(BitmapSurface);
	}

	for (const char *FileName : PieceWhite) {
		BitmapSurface = SDL_LoadBMP(FileName);
		if (BitmapSurface == NULL) return false;
		SDL.Texture.WhitePiece.push_back(SDL_CreateTextureFromSurface(SDL.Renderer, BitmapSurface));
		SDL_FreeSurface(BitmapSurface);
	}

	return true;
}

bool AlderChess::GameRenderer::PrepareApplicationIcon() {
	SDL_Surface *BitmapSurface = NULL;

	BitmapSurface = SDL_LoadBMP("appicon-16x16.bmp");
	if (BitmapSurface == NULL) return false;

	SDL_SetWindowIcon(SDL.Window, BitmapSurface);

	SDL_FreeSurface(BitmapSurface);

	return true;
}

void AlderChess::GameRenderer::Render() {
	SDL_Rect Brush;

	SDL_RenderClear(SDL.Renderer);

	// White background
	SDL_SetRenderDrawColor(SDL.Renderer, 0xff, 0xff, 0xff, 0xff);
	SDL_RenderFillRect(SDL.Renderer, NULL);

	// Icon bar

	Brush.w = Brush.h = UI.Scale * UI.Metric.Icon;

	for (int i : {IconTypeToolbar::Language,
	              IconTypeToolbar::Spotlight,
	              IconTypeToolbar::Restart,
	              IconTypeToolbar::Load,
	              IconTypeToolbar::Save,
	              IconTypeToolbar::Information,
	              IconTypeToolbar::Quit}) {

		XY IconPosition = GetToolbarIconPosition(i);
		Brush.x = IconPosition.x;
		Brush.y = IconPosition.y;

		SDL_RenderCopy(SDL.Renderer, SDL.Texture.Icon[i - 1], NULL, &Brush);

		/* If spotlight has just been turned off and the mouse pointer is
		   still over the spotlight button, then do not highlight spotlight
		   icon. Otherwise highlight spotlight icon if the mouse is over it
		   or if spotlight mode has been enabled. */

		bool HighlightSpotlightIcon =
			(UI.Icon.Sticky != IconTypeToolbar::Spotlight) &&
			((UI.Icon.HoverState.Toolbar[IconTypeToolbar::Spotlight - 1] == true) ||
			Mode.Spotlight.Active);

		/* Highlight the button over which the mouse is on and
		   highlight spotlight button when appropriate. */

		bool HighlightIcon = ((UI.Icon.HoverState.Toolbar[i - 1] == true) &&
		                      (i != IconTypeToolbar::Spotlight))
		                     ||
		                     ((i == IconTypeToolbar::Spotlight) &&
		                      HighlightSpotlightIcon);

		// Dim the save button in promotion mode and in game over state

		bool DimSaveIcon = (i == IconTypeToolbar::Save) &&
		                   (Mode.Promotion || Mode.GameOver);

		if (DimSaveIcon) {
			SDL_SetRenderDrawColor(SDL.Renderer, 0xff, 0xff, 0xff, 0x7f);
			SDL_RenderFillRect(SDL.Renderer, &Brush);
		} else if (HighlightIcon) {
			SDL_SetRenderDrawColor(SDL.Renderer, 0x00, 0x00, 0x00, 0x3f);
			SDL_RenderFillRect(SDL.Renderer, &Brush);
		}
	}

	// Board border (black)
	Brush.x = UI.Scale * UI.Metric.WhiteSpace;
	Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
                          UI.Metric.Icon +
                          UI.Metric.WhiteSpaceSmall);
	Brush.w = Brush.h = UI.Scale * (UI.Metric.BoardBorder +
                                    UI.Metric.Square * 8 +
                                    UI.Metric.BoardBorder);
	SDL_SetRenderDrawColor(SDL.Renderer, 0x00, 0x00, 0x00, 0xff);
	SDL_RenderFillRect(SDL.Renderer, &Brush);

	// Board
	Brush.w = Brush.h = UI.Scale * UI.Metric.Square;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			Brush.x = UI.Scale * (UI.Metric.WhiteSpace +
			                      UI.Metric.BoardBorder +
			                      UI.Metric.Square * x);
			Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
			                      UI.Metric.Icon +
			                      UI.Metric.WhiteSpaceSmall +
			                      UI.Metric.BoardBorder) +
			          UI.Scale * UI.Metric.Square * y;

			// Yellow and blue squares
			if ((x % 2) != (y % 2)) {
				SDL_SetRenderDrawColor(SDL.Renderer, 0x6f, 0x80, 0x9e, 0xff);
			} else {
				SDL_SetRenderDrawColor(SDL.Renderer, 0xd2, 0xcd, 0xb9, 0xff);
			}

			SDL_RenderFillRect(SDL.Renderer, &Brush);

			// Pieces
			if (Logic.GetOwner({x, y}) == PlayerType::Black) {
				SDL_RenderCopy(SDL.Renderer,
				               SDL.Texture.BlackPiece[Logic.GetPiece({x, y}) - 1],
				               NULL,
				               &Brush);

			} else if (Logic.GetOwner({x, y}) == PlayerType::White) {
				SDL_RenderCopy(SDL.Renderer,
				               SDL.Texture.WhitePiece[Logic.GetPiece({x, y}) - 1],
				               NULL,
				               &Brush);

			}

			if (!Mode.GameOver) {
				/* Green overlay on top of squares mark the locations that the
				   player can move the piece to. */
				if (Mode.Select.Active) {
					if (Mode.Select.Map[y][x] > 0) {
						SDL_SetRenderDrawColor(SDL.Renderer, 0x00, 0xff, 0x00, 0x7f);
						SDL_RenderFillRect(SDL.Renderer, &Brush);
					}
				} else if (Mode.Spotlight.Active && !Mode.Promotion) {
					if (Mode.Spotlight.Map[y][x] > 0) {
						/* Yellow overlay on top of squares mark the pieces that
						   the player can move. */
						SDL_SetRenderDrawColor(SDL.Renderer, 0xff, 0xff, 0x00, 0x3f);
						SDL_RenderFillRect(SDL.Renderer, &Brush);
					}
				}
			}
		}
	}

	// Selection marker
	if (Mode.Select.Active) {
		Brush.x = UI.Scale * (UI.Metric.WhiteSpace + UI.Metric.BoardBorder) -
		          UI.Scale * UI.Metric.SelectBorder +
		          UI.Scale * UI.Metric.Square * Mode.Select.SelectedSquare.x;
		Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.Icon +
		                      UI.Metric.WhiteSpaceSmall +
			                  UI.Metric.BoardBorder) -
		          UI.Scale * UI.Metric.SelectBorder +
		          UI.Scale * UI.Metric.Square * Mode.Select.SelectedSquare.y;
		Brush.w = Brush.h = UI.Scale * (UI.Metric.SelectBorder +
		                                UI.Metric.Square +
		                                UI.Metric.SelectBorder);

		SDL_RenderCopy(SDL.Renderer, SDL.Texture.Select, NULL, &Brush);
	}

	// Promotion overlay
	if (Mode.Promotion) {
		// Darken the board
		Brush.x = UI.Scale * (UI.Metric.WhiteSpace + UI.Metric.BoardBorder);
		Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.Icon +
		                      UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.BoardBorder);
		Brush.w = Brush.h = UI.Scale * (UI.Metric.Square * 8);
		SDL_SetRenderDrawColor(SDL.Renderer, 0x00, 0x00, 0x00, 0x7f);
		SDL_RenderFillRect(SDL.Renderer, &Brush);

		// Red overlay for the piece being promoted
		XY PromotionLocation = Logic.QueryPromotionLocation();
		if (PromotionLocation.x != -1) {
			Brush.x = UI.Scale * (UI.Metric.WhiteSpace + UI.Metric.BoardBorder) +
			          UI.Scale * UI.Metric.Square * PromotionLocation.x;
			Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
			                      UI.Metric.Icon +
			                      UI.Metric.WhiteSpaceSmall +
			                      UI.Metric.BoardBorder) +
			          UI.Scale * UI.Metric.Square * PromotionLocation.y;
			Brush.w = Brush.h = UI.Scale * UI.Metric.Square;
			SDL_SetRenderDrawColor(SDL.Renderer, 0xff, 0x00, 0x00, 0x7f);
			SDL_RenderFillRect(SDL.Renderer, &Brush);
		}

		// Pop-up shadow
		Brush.x = UI.Scale * (UI.Metric.WhiteSpace +
		                      UI.Metric.BoardBorder +
		                      UI.Metric.SquarePlusHalf +
		                      UI.Metric.WhiteSpaceTiny);
		Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.Icon +
		                      UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.BoardBorder +
		                      UI.Metric.SquarePlusHalf +
		                      UI.Metric.WhiteSpaceTiny);
		Brush.w = Brush.h = UI.Scale * (UI.Metric.Square * 5);
		SDL_SetRenderDrawColor(SDL.Renderer, 0x00, 0x00, 0x00, 0xbf);
		SDL_RenderFillRect(SDL.Renderer, &Brush);

		// Red pop-up
		Brush.x = UI.Scale * (UI.Metric.WhiteSpace +
		                      UI.Metric.BoardBorder +
		                      UI.Metric.SquarePlusHalf);
		Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.Icon +
		                      UI.Metric.WhiteSpaceSmall +
		                      UI.Metric.BoardBorder +
		                      UI.Metric.SquarePlusHalf);
		Brush.w = Brush.h = UI.Scale * (UI.Metric.Square * 5);
		SDL_SetRenderDrawColor(SDL.Renderer, 0xcf, 0x00, 0x00, 0xff);
		SDL_RenderFillRect(SDL.Renderer, &Brush);

		// Pieces (knight, bishop, rook, dame)
		Brush.w = Brush.h = UI.Scale * UI.Metric.Square * 2;
		int l = 0;
		for (int p : {PieceType::Knight,
		              PieceType::Bishop,
		              PieceType::Rook,
		              PieceType::Dame}) {

			int x = l % 2;
			int y = l / 2;

			Brush.x = UI.Scale * (UI.Metric.WhiteSpace +
			                      UI.Metric.BoardBorder +
			                      UI.Metric.Square * 2) +
			          UI.Scale * (UI.Metric.Square * x * 2);
			Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
			                      UI.Metric.Icon +
			                      UI.Metric.WhiteSpaceSmall +
			                      UI.Metric.BoardBorder +
			                      UI.Metric.Square * 2) +
			          UI.Scale * (UI.Metric.Square * y * 2);

			if (Logic.GetTurn() == PlayerType::Black) {
				SDL_RenderCopy(SDL.Renderer, SDL.Texture.BlackPiece[p - 1], NULL, &Brush);
			} else if (Logic.GetTurn() == PlayerType::White) {
				SDL_RenderCopy(SDL.Renderer, SDL.Texture.WhitePiece[p - 1], NULL, &Brush);
			}

			if (UI.Icon.HoverState.Promotion[l] == true) {
				SDL_SetRenderDrawColor(SDL.Renderer, 0x00, 0x00, 0x00, 0x3f);
				SDL_RenderFillRect(SDL.Renderer, &Brush);
			}

			l++;
		}
	}

	// Info text
	Brush.x = UI.Scale * UI.Metric.WhiteSpace;
	Brush.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
	                      UI.Metric.Icon +
	                      UI.Metric.WhiteSpaceSmall +
	                      UI.Metric.BoardBorder +
	                      UI.Metric.Square * 8 +
	                      UI.Metric.BoardBorder +
	                      UI.Metric.WhiteSpace);
	Brush.w = UI.WindowSize.x - 2 * UI.Scale * UI.Metric.WhiteSpace;
	Brush.h = UI.Scale * UI.Metric.Text;

	// Hover label takes higher priority than static label
	if (UI.Label.Hover != LabelType::NoLabel) {

		SDL_RenderCopy(SDL.Renderer,
		               Text->GetLabelTexture(UI.Language, UI.Label.Hover),
		               NULL,
		               &Brush);

	} else if (UI.Label.Static != LabelType::NoLabel) {

		SDL_RenderCopy(SDL.Renderer,
		               Text->GetLabelTexture(UI.Language, UI.Label.Static),
		               NULL,
		               &Brush);

	}

	SDL_RenderPresent(SDL.Renderer);
}

AlderChess::XY AlderChess::GameRenderer::ComputeWindowSize() {
	XY WindowSize;

	WindowSize.x = UI.Scale * (UI.Metric.WhiteSpace +
	                           UI.Metric.BoardBorder +
	                           UI.Metric.Square * 8 +
	                           UI.Metric.BoardBorder +
	                           UI.Metric.WhiteSpace);

	WindowSize.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
	                           UI.Metric.Icon +
	                           UI.Metric.WhiteSpaceSmall +
	                           UI.Metric.BoardBorder +
	                           UI.Metric.Square * 8 +
	                           UI.Metric.BoardBorder +
	                           UI.Metric.WhiteSpace +
	                           UI.Metric.Text +
	                           UI.Metric.WhiteSpace);
	
	return WindowSize;
}

void AlderChess::GameRenderer::HandleMouseClick(XY Coordinates) {
	int MouseClickIconToolbar = GetMouseIconToolbar(Coordinates);
	int MouseClickIconPromotion = GetMouseIconPromotion(Coordinates);
	XY MouseClickSquare = GetMouseSquare(Coordinates);

	if (MouseClickIconToolbar != IconTypeToolbar::NoIcon) {
		Mode.Select.Active = false;

		switch (MouseClickIconToolbar) {
			case IconTypeToolbar::Language:
				SwitchUILanguage();

				UI.Icon.Sticky = IconTypeToolbar::Language;
				UI.Label.Hover = LabelType::NativeName;

				break;

			case IconTypeToolbar::Spotlight:
				Mode.Spotlight.Active = !Mode.Spotlight.Active;

				/* When user turns spotlight mode off, it provides better
				   UI feedback if the spotlight icon is not highlighted
				   when the mouse is still over it. Therefore the spotlight
				   button is also set as sticky, even though there is no
				   change in label. */

				if (!Mode.Spotlight.Active) {
					UI.Icon.Sticky = IconTypeToolbar::Spotlight;
				} else {
					UI.Icon.Sticky = IconTypeToolbar::NoIcon;
				}

				break;

			case IconTypeToolbar::Restart:
				Logic.Restart();

				Mode.GameOver = false;
				Mode.Promotion = false;

				UpdateSpotlightMap();
				UpdateTurnLabel();

				break;

			case IconTypeToolbar::Save:
				if (!Mode.Promotion && !Mode.GameOver) {
					if (Logic.Save()) {
						UI.Icon.Sticky = IconTypeToolbar::Save;
						UI.Label.Hover = LabelType::SaveSuccess;
					} else {
						UI.Icon.Sticky = IconTypeToolbar::Save;
						UI.Label.Hover = LabelType::SaveFailure;
					}
				}

				break;

			case IconTypeToolbar::Load:
				if (Logic.Load()) {
					UI.Icon.Sticky = IconTypeToolbar::Load;
					UI.Label.Hover = LabelType::LoadSuccess;

					Mode.GameOver = false;
					Mode.Promotion = false;

					UpdateSpotlightMap();
					UpdateTurnLabel();
				} else {
					UI.Icon.Sticky = IconTypeToolbar::Load;
					UI.Label.Hover = LabelType::LoadFailure;
				}

				break;

			case IconTypeToolbar::Information:
				ShowInformationDialog();
				break;

			case IconTypeToolbar::Quit:
				QuitApplication = true;
				break;
		}
	} else if (Mode.Promotion) {
		int PromotionSelection = TranslatePromotionIconToPiece(MouseClickIconPromotion);
		if (PromotionSelection != PieceType::NoPiece) {
			Logic.SatisfyPromotionRequest(PromotionSelection);
			Mode.Promotion = false;
			AfterTurnTasks();
		}
	} else if (MouseClickSquare.InRange8()) {
		if (Mode.Select.Active) {
			if (MouseClickSquare == Mode.Select.SelectedSquare) {
				// If the player clicks the already selected piece, it is unselected
				Mode.Select.Active = false;
			} else {
				// Move to target square if possible
				if (Logic.Move(Mode.Select.SelectedSquare, MouseClickSquare)) {
					Mode.Select.Active = false;

					if (Logic.PromotionRequested()) {
						// When promotion mode is entered, AfterTurnTasks() is postponed
						UI.Label.Static = LabelType::Promotion;
						Mode.Promotion = true;
					} else {
						AfterTurnTasks();
					}
				}
			}
		} else if (!Mode.GameOver) {
			if (Logic.CanSelect(Logic.GetTurn(), MouseClickSquare)) {
				Mode.Select.Map = EmptyMatrix8x8();

				Mode.Select.SelectedSquare = MouseClickSquare;
				Mode.Select.Map = Logic.GetTravelMap(Logic.GetTurn(),
				                                     Mode.Select.SelectedSquare,
				                                     true, true, false, true, false);
				Mode.Select.Active = true;
			}
		}
	}

	return;
}

#ifdef _DEBUG
	void AlderChess::GameRenderer::HandleMouseClickRight(XY Coordinates) {
		XY MouseClickSquare = GetMouseSquare(Coordinates);
		if (MouseClickSquare.InRange8()) {
			Mode.Select.Active = false;
			Logic.SetOwnerDebug(MouseClickSquare, PlayerType::Nobody);
			Logic.SetPieceDebug(MouseClickSquare, PieceType::NoPiece);
			UpdateSpotlightMap();
		}

		return;
	}
#endif

void AlderChess::GameRenderer::HandleMouseHover(XY Coordinates) {
	// Update hover state of icons

	int MouseOverIcon = 0;

	for (int i = 0; i < 7; i++) {
		UI.Icon.HoverState.Toolbar[i] = false;
	}

	for (int i = 0; i < 4; i++) {
		UI.Icon.HoverState.Promotion[i] = false;
	}

	MouseOverIcon = GetMouseIconToolbar(Coordinates);

	/* Do not apply a new hover label if a sticky label has not been dismissed
	   by moving the mouse pointer away from the associated icon. */

	if (UI.Icon.Sticky == IconTypeToolbar::NoIcon) {
		// No sticky icon present
		UI.Icon.Hover = MouseOverIcon;
	} else if (MouseOverIcon != UI.Icon.Sticky) {
		/* Mouse pointer has been moved away from sticky icon,
		   so reset sticky icon information. */
		UI.Icon.Sticky = IconTypeToolbar::NoIcon;
	}

	if (MouseOverIcon == IconTypeToolbar::NoIcon) {
		UI.Label.Hover = LabelType::NoLabel;
	} else {
		if (UI.Icon.Sticky == IconTypeToolbar::NoIcon) {
			switch (MouseOverIcon) {
				case IconTypeToolbar::Language:
					UI.Label.Hover = LabelType::Language;
					break;
				case IconTypeToolbar::Spotlight:
					UI.Label.Hover = LabelType::Spotlight;
					break;
				case IconTypeToolbar::Restart:
					UI.Label.Hover = LabelType::Restart;
					break;
				case IconTypeToolbar::Load:
					UI.Label.Hover = LabelType::Load;
					break;
				case IconTypeToolbar::Save:
					UI.Label.Hover = LabelType::Save;
					break;
				case IconTypeToolbar::Information:
					UI.Label.Hover = LabelType::Information;
					break;
				case IconTypeToolbar::Quit:
					UI.Label.Hover = LabelType::Quit;
					break;
			}
		}

		UI.Icon.HoverState.Toolbar[MouseOverIcon - 1] = true;
	}

	MouseOverIcon = GetMouseIconPromotion(Coordinates);

	if (MouseOverIcon != IconTypePromotion::PromotionNo) {
		UI.Icon.HoverState.Promotion[MouseOverIcon - 1] = true;
	}

	return;
}

int AlderChess::GameRenderer::GetMouseIconToolbar(XY Coordinates) {
	// Report the toolbar icon over which the mouse is on

	XY Test;

	int Icon = 1;

	do {
		Test.x = 0;

		Test = GetToolbarIconPosition(Icon);

		if (Coordinates.InRange({Test.x,
		                         Test.y},
		                        {Test.x + UI.Scale * UI.Metric.Icon,
		                         Test.y + UI.Scale * UI.Metric.Icon})) {
			return Icon;
		}
	} while(++Icon < 8);

	return IconTypeToolbar::NoIcon;
}

int AlderChess::GameRenderer::GetMouseIconPromotion(XY Coordinates) {
	// Report the promotion piece over which the mouse is on

	if (!Mode.Promotion) return IconTypePromotion::PromotionNo;

	for (int p = 0; p < 4; p++) {
		XY LeftTop;
		XY RightBottom;

		int x = p % 2;
		int y = p / 2;

		LeftTop.x = UI.Scale * (UI.Metric.WhiteSpace +
		                        UI.Metric.BoardBorder +
		                        UI.Metric.Square * 2) +
		            UI.Scale * (UI.Metric.Square * x * 2);
		LeftTop.y = UI.Scale * (UI.Metric.WhiteSpaceSmall +
		                        UI.Metric.Icon +
		                        UI.Metric.WhiteSpaceSmall +
		                        UI.Metric.BoardBorder +
		                        UI.Metric.Square * 2) +
		                        UI.Scale * (UI.Metric.Square * y * 2);

		RightBottom.x = LeftTop.x + UI.Scale * UI.Metric.Square * 2;
		RightBottom.y = LeftTop.y + UI.Scale * UI.Metric.Square * 2;

		if (Coordinates.InRange(LeftTop, RightBottom)) {
			return p + 1;
		}
	}

	return IconTypePromotion::PromotionNo;
}

AlderChess::XY AlderChess::GameRenderer::GetMouseSquare(XY Coordinates) {
	// Report the board square over which the mouse is on

	XY LeftTop;
	XY RightBottom;
	XY Result;

	int PaddingLeft = UI.Scale * (UI.Metric.WhiteSpace +
	                              UI.Metric.BoardBorder);
	int PaddingTop = UI.Scale * (UI.Metric.WhiteSpaceSmall +
	                             UI.Metric.Icon +
	                             UI.Metric.WhiteSpaceSmall +
	                             UI.Metric.BoardBorder);

	int SquareSizeReal = UI.Scale * UI.Metric.Square;

	LeftTop.x = PaddingLeft;
	LeftTop.y = PaddingTop;
	RightBottom.x = LeftTop.x + SquareSizeReal * 8;
	RightBottom.y = LeftTop.y + SquareSizeReal * 8;

	if (!Coordinates.InRange(LeftTop, RightBottom)) {
		// Not valid coordinate (the click was outside the board)
		Result = {-1, -1};
	} else {
		Result.x = (Coordinates.x - PaddingLeft) / SquareSizeReal;
		Result.y = (Coordinates.y - PaddingTop) / SquareSizeReal;
	}

	return Result;
}

void AlderChess::GameRenderer::ShowInformationDialog() {
	std::string InformationText =
		"Alder Chess\n\n"s +
		"Version "s + std::to_string(Version) + "\n"s +
		"Running on "s + SDL_GetPlatform() + "\n\n"s +
		"(C) MicroLabs\n"s +
		"(C) Joonas Saarinen\n\n"s +
		"This program is licensed under GNU General Public License (version 3).\n"s +
		"Please see the included file gpl-3.0.txt for more details.\n\n"s +
		"Web: microlabs.fi"s;

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                             "About Alder Chess",
                             InformationText.c_str(),
                             SDL.Window);

	return;
}

void AlderChess::GameRenderer::SwitchUILanguage() {
	int LanguageFirst = LanguageType::English;
	int LanguageLast = LanguageType::Finnish;

	UI.Language++;

	if (UI.Language > LanguageLast) UI.Language = LanguageFirst;

	return;
}

void AlderChess::GameRenderer::UpdateTurnLabel() {
	if (Logic.GetTurn() == PlayerType::Black) {
		UI.Label.Static = LabelType::TurnBlack;
	} else if (Logic.GetTurn() == PlayerType::White) {
		UI.Label.Static = LabelType::TurnWhite;
	} else {
		UI.Label.Static = LabelType::NoLabel;
	}

	return;
}

void AlderChess::GameRenderer::AfterTurnTasks() {
	Logic.ChangeTurn();

	UpdateSpotlightMap();
	UpdateTurnLabel();

	if (Logic.Checkmate(Logic.GetTurn())) {

		if (Logic.GetTurn() == PlayerType::Black) {
			UI.Label.Static = LabelType::CheckmateWinnerWhite;
		} else if (Logic.GetTurn() == PlayerType::White) {
			UI.Label.Static = LabelType::CheckmateWinnerBlack;
		}

		Mode.GameOver = true;

	} else if (Logic.Stalemate(Logic.GetTurn())) {

		if (Logic.GetTurn() == PlayerType::Black) {
			UI.Label.Static = LabelType::StalemateBlack;
		} else if (Logic.GetTurn() == PlayerType::White) {
			UI.Label.Static = LabelType::StalemateWhite;
		}

		Mode.GameOver = true;

	}

	return;
}

int AlderChess::GameRenderer::TranslatePromotionIconToPiece(int Icon) {
	// Translate clicked icon of promotion overlay to actual piece type

	int i = 0;

	for (int p : {PieceType::NoPiece,
	              PieceType::Knight,
	              PieceType::Bishop,
	              PieceType::Rook,
	              PieceType::Dame}) {
		if (i++ == Icon) return p;
	}

	return PieceType::NoPiece;
}

AlderChess::XY AlderChess::GameRenderer::GetToolbarIconPosition(int Icon) {
	/* Returns the XY coordinate of the top left corner of
	   the specified toolbar icon. */

	XY Result;

	Result.x = 0;
	Result.y = UI.Scale * UI.Metric.WhiteSpaceSmall;

	int FromLeft = 0;
	int FromRight = 0 + 1;

	switch (Icon) {
		case IconTypeToolbar::Spotlight: FromLeft++;
		case IconTypeToolbar::Language:
			Result.x = UI.Scale * UI.Metric.WhiteSpace +
				       FromLeft * UI.Scale * UI.Metric.Icon;
			break;

		case IconTypeToolbar::Restart: FromRight++;
		case IconTypeToolbar::Load: FromRight++;
		case IconTypeToolbar::Save: FromRight++;
		case IconTypeToolbar::Information: FromRight++;
		case IconTypeToolbar::Quit:
			Result.x = UI.WindowSize.x -
				       UI.Scale * UI.Metric.WhiteSpace -
				       FromRight * UI.Scale * UI.Metric.Icon;
			break;
	}

	return Result;
}

void AlderChess::GameRenderer::UpdateSpotlightMap() {
	Matrix<int, 8, 8> TravelMap = EmptyMatrix8x8();
	Mode.Spotlight.Map = EmptyMatrix8x8();

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			if (Logic.GetOwner({x, y}) == Logic.GetTurn()) {
				TravelMap = Logic.GetTravelMap(Logic.GetTurn(),
				                               {x, y},
				                               true, true, false, true, false);

				bool Mobile = false;

				for (int ty = 0; ty < 8; ty++) {
					for (int tx = 0; tx < 8; tx++) {
						if (TravelMap[ty][tx] > 0) Mobile = true;
					}
				}

				if (Mobile) {
					Mode.Spotlight.Map[y][x] = 1;
				} else {
					Mode.Spotlight.Map[y][x] = 0;
				}
			}
		}
	}

	return;
}