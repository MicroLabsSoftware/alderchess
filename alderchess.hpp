/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#pragma once

#include "stdafx.hpp"

namespace AlderChess {
	const int Version = 1;

	class XY {
		public:
			int x = 0;
			int y = 0;
			XY(){};
			XY(int X, int Y);

			bool operator==(const XY& other);
			const XY& operator=(const XY& other);
			XY operator+(const XY& other) const;
			XY& operator+=(const XY& other);
			XY operator-(const XY& other) const;
			XY& operator-=(const XY& other);

			bool InRange(XY a, XY b);
			bool InRange8();
	};

	void ShowError(std::string Text);
	Matrix<int, 8, 8> EmptyMatrix8x8();
	Matrix<int, 8, 8> NormalizeMatrix8x8 (Matrix<int, 8, 8> mtx);

	enum PlayerType { Nobody, Black, White };
	enum PieceType { NoPiece, Pawn, Knight, Bishop, Rook, Dame, King };
	enum LanguageType { English, Korean, Finnish };
	enum LabelType { NoLabel,
	                 NativeName,
	                 Language,
	                 Spotlight,
	                 Restart,
	                 Load,
	                 Save,
	                 Quit,
	                 Promotion,
	                 TurnBlack,
	                 TurnWhite,
	                 CheckmateWinnerBlack,
	                 CheckmateWinnerWhite,
	                 StalemateBlack,
	                 StalemateWhite,
	                 LoadSuccess,
	                 SaveSuccess,
	                 LoadFailure,
	                 SaveFailure };

	class GameLogic {
		public:
			GameLogic();
			void Restart();
			bool Save();
			bool Load();

			Matrix<int, 8, 8> GetTravelMap(int Player,
			                               XY SquareCoordinates,
			                               bool SubstractKingIllegal,
			                               bool IncludeSpecialMoves,
			                               bool PreventRecursiveKingInCheck,
			                               bool IncludeKing,
			                               bool AlwaysIncludePawnDiagonal);

			Matrix<int, 8, 8> GetTravelMapGlobal(int Player,
			                                     bool PreventRecursiveKingInCheck,
			                                     bool IncludeKing,
			                                     bool AlwaysIncludePawnDiagonal);
			
			int GetPiece(XY Coordinates);
			int GetOwner(XY Coordinates);

			void SetPieceDebug(XY Coordinates, int Piece);
			void SetOwnerDebug(XY Coordinates, int Owner);

			bool CanSelect(int Player, XY SquareCoordinates);
			int Opponent(int Player);
			int GetTurn();
			void ChangeTurn();
			bool Move(XY Source, XY Destination);
			bool Checkmate(int Player);
			bool Stalemate(int Player);
			bool PromotionRequested();
			XY QueryPromotionLocation();
			void SatisfyPromotionRequest(int Piece);

		private:
			struct BoardPiece {
				int Owner = PlayerType::Nobody;
				int Piece = PieceType::NoPiece;
				bool Touched = false;
				bool EnPassantTarget = false;
				bool CastlingTarget = false;
			};

			Matrix<BoardPiece, 8, 8> Board;

			struct {
				bool Pending = false;
				XY Location = {-1, -1};
			} Promotion;

			int Turn = PlayerType::White;

			Matrix<int, 8, 8> Travel(int Player,
			                         XY Location,
			                         XY SpeedVector,
			                         int Distance);

			XY FindKing(int Player);
			bool KingInCheck(int Player);
			bool PlayerCanMove(int Player);
			void SwapPieces(XY Source, XY Destination);
			void UpdateCastlingInformation(int Player);
	};

	class GameText {
		public:
			GameText(std::pair<std::string, int> FaceWestern,
			         std::pair<std::string, int> FaceHangul,
			         XY LabelSize,
			         unsigned int DPI,
			         SDL_Renderer* Renderer);
			~GameText();
			bool good();
			SDL_Texture* GetLabelTexture(int Language, int LabelID);
	
		private:
			void CompositeCharacter(SDL_Surface* Surface,
			                        FT_GlyphSlot* Glyph,
			                        XY Pen);
			std::wstring GetText(int Language, int LabelID);
			std::vector<std::vector<SDL_Texture*>> Labels;

			bool Good = false;
	};

	class GameRenderer {
		public:
			GameRenderer();
			~GameRenderer();
			bool good();
			std::string GetErrorMessage();
			bool WantsToQuit();

			void Render();
			void HandleMouseClick(XY Coordinates);
			#ifdef _DEBUG
				void HandleMouseClickRight(XY Coordinates);
			#endif
			void HandleMouseHover(XY Coordinates);

		private:
			bool Good = false;
			bool QuitApplication = false;
			std::string ErrorMessage;

			AlderChess::GameLogic Logic;

			/* The Text object is placed behind a pointer so that a late
			   construction and early destruction can be performed on it. */
			std::unique_ptr<AlderChess::GameText> Text;

			enum IconTypeToolbar { NoIcon,
			                       Language,
			                       Spotlight,
			                       Restart,
			                       Load,
			                       Save,
			                       Quit };

			enum IconTypePromotion { PromotionNo,
			                         PromotionKnight,
			                         PromotionBishop,
			                         PromotionRook,
			                         PromotionDame };

			struct {
				SDL_Window *Window = NULL;
				int WindowDisplayIndex = -1;
				SDL_Renderer *Renderer = NULL;
				struct {
					SDL_Texture *Select = NULL;
					std::vector<SDL_Texture*> Icon;
					std::vector<SDL_Texture*> BlackPiece;
					std::vector<SDL_Texture*> WhitePiece;
				} Texture;
			} SDL;

			/* The idea of a "sticky icon" is that when it is clicked,
			   it produces a result text, and we don't want that text
			   to disappear until the mouse pointer is hovered away from
			   the icon. When the pointer is hovered again over that same
			   icon, then the ordinary text for that icon appears again. */

			struct {
				XY WindowSize;
				int Scale = 1;
				int Language = LanguageType::Korean;

				struct {
					// Sizes at 50 DPI
					const int WhiteSpace = 8;
					const int WhiteSpaceSmall = 4;
					const int WhiteSpaceTiny = 2;
					const int Square = 16;
					const int SquarePlusHalf = 24;
					const int BoardBorder = 8;
					const int Text = 14;
					const int Icon = 16;
					const int SelectBorder = 2;

					// FreeType will calculate font sizes on all DPI
					const int FontSizeWestern = 16;
					const int FontSizeHangul = 18;
				} Metric;

				struct {
					int Hover = LabelType::NoLabel;
					int Static = LabelType::NoLabel;
				} Label;

				struct {
					struct {
						bool Toolbar[6] = { false };
						bool Promotion[4] = { false };
					} HoverState;

					int Hover = IconTypeToolbar::NoIcon;
					int Sticky = IconTypeToolbar::NoIcon;
				} Icon;
			} UI;

			struct {
				bool GameOver = false;

				bool Promotion = false;

				struct {
					bool Active = false;
					Matrix<int, 8, 8> Map = EmptyMatrix8x8();
				} Spotlight;
				
				struct {
					bool Active = false;
					XY SelectedSquare;
					Matrix<int, 8, 8> Map = EmptyMatrix8x8();
				} Select;
			} Mode;

			void SetErrorMessage(std::string Message);
			bool LoadResources();
			void SwitchUILanguage();
			void UpdateTurnLabel();
			void AfterTurnTasks();
			int TranslatePromotionIconToPiece(int Icon);
			XY ComputeWindowSize();
			int GetMouseIconToolbar(XY Coordinates);
			int GetMouseIconPromotion(XY Coordinates);
			XY GetMouseSquare(XY Coordinates);
			XY GetToolbarIconPosition(int Icon);
			void UpdateSpotlightMap();
	};

	class EventManager {
		public:
			EventManager();
			bool good();
			void Run();

		private:
			bool Good = false;

			struct {
				SDL_Event Event;
			} SDL;

			AlderChess::GameRenderer Renderer;
	};
}