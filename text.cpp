/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#include "stdafx.hpp"
#include "alderchess.hpp"

AlderChess::GameText::GameText(std::pair<std::string, int> FaceWestern,
                               std::pair<std::string, int> FaceHangul,
                               XY LabelSize,
                               unsigned int DPI,
                               SDL_Renderer* Renderer) {
	// Prerenders labels for all languages as SDL texture pointers

	struct {
		FT_Library Library;
		struct { FT_Face Western, Hangul; } Face;
		FT_Error Error;
	} FreeType;

	FreeType.Error = FT_Init_FreeType(&FreeType.Library);
	if (FreeType.Error) return;

    FreeType.Error = FT_New_Face(FreeType.Library,
	                             FaceWestern.first.c_str(),
	                             0,
	                             &FreeType.Face.Western);
    if (FreeType.Error) return;
    
    FreeType.Error = FT_Set_Char_Size(FreeType.Face.Western,
	                                  0,
	                                  FaceWestern.second * 64,
	                                  DPI, DPI);
    if (FreeType.Error) return;

    FreeType.Error = FT_New_Face(FreeType.Library,
	                             FaceHangul.first.c_str(),
	                             0,
	                             &FreeType.Face.Hangul);
    if (FreeType.Error) return;
    
    FreeType.Error = FT_Set_Char_Size(FreeType.Face.Hangul,
	                                  0,
	                                  FaceHangul.second * 64,
	                                  DPI, DPI);
    if (FreeType.Error) return;

	for (int l : { LanguageType::English,
	               LanguageType::Korean,
	               LanguageType::Finnish }) {

		// Create empty language node
		Labels.push_back(std::vector<SDL_Texture*>());

		Uint32 RMask, GMask, BMask, AMask;

		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			RMask = 0xff000000;
			GMask = 0x00ff0000;
			BMask = 0x0000ff00;
			AMask = 0x000000ff;
		#else
			RMask = 0x000000ff;
			GMask = 0x0000ff00;
			BMask = 0x00ff0000;
			AMask = 0xff000000;
		#endif

		for (int t : { LabelType::NoLabel,
	                   LabelType::NativeName,
	                   LabelType::Language,
		               LabelType::Spotlight,
	                   LabelType::Restart,
	                   LabelType::Load,
	                   LabelType::Save,
	                   LabelType::Quit,
	                   LabelType::Promotion,
	                   LabelType::TurnBlack,
	                   LabelType::TurnWhite,
	                   LabelType::CheckmateWinnerBlack,
	                   LabelType::CheckmateWinnerWhite,
	                   LabelType::StalemateBlack,
	                   LabelType::StalemateWhite,
	                   LabelType::LoadSuccess,
	                   LabelType::SaveSuccess,
	                   LabelType::LoadFailure,
	                   LabelType::SaveFailure }) {
			SDL_Surface* Surface = SDL_CreateRGBSurface(0,
			                                            LabelSize.x, LabelSize.y,
			                                            32,
			                                            RMask, GMask, BMask, AMask);
			if (Surface == NULL) return;

			// White background and fully opaque translucency
			std::memset(Surface->pixels, 0xff, LabelSize.x * LabelSize.y * 4);

			FT_Face* ActiveFace = NULL;
			int BottomLine = Surface->h - 1;

			if (l == LanguageType::Korean) {
				ActiveFace = &FreeType.Face.Hangul;
				BottomLine = (int)std::round((float)Surface->h * (4.0f / 5.0f));
			} else {
				ActiveFace = &FreeType.Face.Western;
				BottomLine = (int)std::round((float)Surface->h * (3.0f / 5.0f));
			}

			XY Pen = {0, 0};

			for (FT_ULong c : GetText(l, t)) {
				FT_UInt GlyphIndex = FT_Get_Char_Index(*ActiveFace, c);

				// The characters that could not be rendered are skipped

				FreeType.Error = FT_Load_Glyph(*ActiveFace,
				                               GlyphIndex,
				                               FT_LOAD_DEFAULT);
				if (FreeType.Error) continue;

				FreeType.Error = FT_Render_Glyph((*ActiveFace)->glyph,
				                                 FT_RENDER_MODE_NORMAL);
				if (FreeType.Error) continue;

				CompositeCharacter(Surface,
				                   &((*ActiveFace)->glyph),
				                   {Pen.x,
				                    Pen.y + (BottomLine - (*ActiveFace)->glyph->bitmap_top)});

				Pen.x += (*ActiveFace)->glyph->advance.x >> 6;
			}

			SDL_Texture* Texture = SDL_CreateTextureFromSurface(Renderer, Surface);
			if (Texture == NULL) return;
			Labels[l].push_back(Texture);
			SDL_FreeSurface(Surface);
		}
	}

	FT_Done_FreeType(FreeType.Library);

	Good = true;
}

AlderChess::GameText::~GameText() {
	for (std::vector<SDL_Texture*> l : Labels) {
		for (SDL_Texture* t : l) {
			if (t != NULL) SDL_DestroyTexture(t);
		}
	}
}

bool AlderChess::GameText::good() {
	return Good;
}

void AlderChess::GameText::CompositeCharacter(SDL_Surface* Surface,
                                              FT_GlyphSlot* Glyph,
                                              XY Pen) {

	/* Copies a character from a FreeType glyph framebuffer to an SDL surface
	   in the specified position. */

	XY PixelParent;
	XY PixelChild;

	for (PixelChild.y = 0;
			PixelChild.y < (int)(*Glyph)->bitmap.rows;
			PixelChild.y++) {
		for (PixelChild.x = 0;
		     PixelChild.x < (int)(*Glyph)->bitmap.width;
		     PixelChild.x++) {
			PixelParent.x = Pen.x +
			                PixelChild.x +
			                (*Glyph)->bitmap_left;
			PixelParent.y = Pen.y +
			                PixelChild.y;

			// Do not render outside the framebuffer
			if (!PixelParent.InRange({0, 0}, {Surface->w, Surface->h}))
				continue;

			int OffsetChild = (*Glyph)->bitmap.width * PixelChild.y +
			                  PixelChild.x;

			int OffsetParent = (Surface->w * PixelParent.y + PixelParent.x) * 4;

			unsigned char Monochrome = (*Glyph)->bitmap.buffer[OffsetChild];

			for (int h = 0; h < 3; h++) {
				*((unsigned char *)Surface->pixels + OffsetParent + h) -= Monochrome;
			}
		}
	}

	return;
}

SDL_Texture* AlderChess::GameText::GetLabelTexture(int Language, int LabelID) {
	return Labels[Language][LabelID];
}

std::wstring AlderChess::GameText::GetText(int Language, int LabelID) {
	if (LabelID == LabelType::NoLabel) return L"";

	switch (Language) {
		case LanguageType::English:
			switch (LabelID) {
				case LabelType::NativeName: return L"English";
				case LabelType::Language: return L"Language";
				case LabelType::Spotlight: return L"Spotlight";
				case LabelType::Restart: return L"Restart";
				case LabelType::Load: return L"Load";
				case LabelType::Save: return L"Save";
				case LabelType::Quit: return L"Quit";
				case LabelType::Promotion: return L"Promotion";
				case LabelType::TurnBlack: return L"Black's turn.";
				case LabelType::TurnWhite: return L"White's turn.";
				case LabelType::CheckmateWinnerBlack: return L"Checkmate! Black wins.";
				case LabelType::CheckmateWinnerWhite: return L"Checkmate! White wins.";
				case LabelType::StalemateBlack: return L"Stalemate for black! It's a draw.";
				case LabelType::StalemateWhite: return L"Stalemate for white! It's a draw.";
				case LabelType::LoadSuccess: return L"Game loaded.";
				case LabelType::SaveSuccess: return L"Game saved.";
				case LabelType::LoadFailure: return L"Could not load game successfully.";
				case LabelType::SaveFailure: return L"Could not save game successfully.";
				default: return L"";
			}
		case LanguageType::Korean:
			switch (LabelID) {
				case LabelType::NativeName: return L"\uD55C\uAD6D\uC5B4";
				case LabelType::Language: return L"\uC5B8\uC5B4";
				case LabelType::Spotlight: return L"\uD2B9\uD788 \uBC1D\uAC8C \uD558\uB2E4";
				case LabelType::Restart: return L"\uC0C8\uB85C\uC2DC\uC791";
				case LabelType::Load: return L"\uBD88\uB7EC\uC624\uAE30";
				case LabelType::Save: return L"\uC800\uC7A5\uD558\uAE30";
				case LabelType::Quit: return L"\uAC8C\uC784 \uB05D";
				case LabelType::Promotion: return L"\uD504\uB85C\uBAA8\uC158";
				case LabelType::TurnBlack: return L"\uD751\uC0C9\uC758 \uCC28\uB808\uC785\uB2C8\uB2E4.";
				case LabelType::TurnWhite: return L"\uBC31\uC0C9\uC758 \uCC28\uB808\uC785\uB2C8\uB2E4.";
				case LabelType::CheckmateWinnerBlack: return L"\uC678\uD1B5\uC218! \uD751\uC0C9\uC740 \uC2B9\uC790\uC785\uB2C8\uB2E4.";
				case LabelType::CheckmateWinnerWhite: return L"\uC678\uD1B5\uC218! \uBC31\uC0C9\uC740 \uC2B9\uC790\uC785\uB2C8\uB2E4.";
				case LabelType::StalemateBlack: return L"\uD751\uC0C9\uC740 \uC2A4\uD14C\uC77C\uBA54\uC774\uD2B8\uB97C \uC788\uC5B4\uC694! \uBB34\uC2B9\uBD80.";
				case LabelType::StalemateWhite: return L"\uBC31\uC0C9\uC740 \uC2A4\uD14C\uC77C\uBA54\uC774\uD2B8\uB97C \uC788\uC5B4\uC694! \uBB34\uC2B9\uBD80.";
				case LabelType::LoadSuccess: return L"\uBD88\uB7EC \uC131\uACF5\uD588\uB2E4.";
				case LabelType::SaveSuccess: return L"\uC800\uC7A5 \uC131\uACF5\uD588\uB2E4.";
				case LabelType::LoadFailure: return L"\uBD88\uB7EC \uC218 \uC5C6\uB2E4.";
				case LabelType::SaveFailure: return L"\uC800\uC7A5 \uC218 \uC5C6\uB2E4.";
				default: return L"";
			}
		case LanguageType::Finnish:
			switch (LabelID) {
				case LabelType::NativeName: return L"Suomi";
				case LabelType::Language: return L"Kieli";
				case LabelType::Spotlight: return L"Parrasvalot";
				case LabelType::Restart: return L"Aloita alusta";
				case LabelType::Load: return L"Lataa peli";
				case LabelType::Save: return L"Tallenna peli";
				case LabelType::Quit: return L"Poistu";
				case LabelType::Promotion: return L"Promootio";
				case LabelType::TurnBlack: return L"Mustan vuoro.";
				case LabelType::TurnWhite: return L"Valkoisen vuoro.";
				case LabelType::CheckmateWinnerBlack: return L"Shakkimatti! Musta on voittaja.";
				case LabelType::CheckmateWinnerWhite: return L"Shakkimatti! Valkoinen on voittaja.";
				case LabelType::StalemateBlack: return L"Musta on pattitilanteessa! Tasapeli.";
				case LabelType::StalemateWhite: return L"Valkoinen on pattitilanteessa! Tasapeli.";
				case LabelType::LoadSuccess: return L"Pelitilanne ladattu.";
				case LabelType::SaveSuccess: return L"Pelitilanne tallennettu.";
				case LabelType::LoadFailure: return L"Pelitilanteen lataaminen ep\u00E4onnistui.";
				case LabelType::SaveFailure: return L"Pelitilanteen tallentaminen ep\u00E4onnistui.";
				default: return L"";
			}
		default: return L"";
	}
	return L"";
}