/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#include "stdafx.hpp"
#include "alderchess.hpp"

AlderChess::GameLogic::GameLogic() {
	Restart();
}

void AlderChess::GameLogic::Restart() {
	// Set default values

	Turn = PlayerType::White;
	Promotion.Pending = false;
	Promotion.Location = {0, 0};

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
				// Touched applies only to pieces
				if (y <= 1 || y >= 6) {
					Board[y][x].Touched = false;
				} else {
					Board[y][x].Touched = true;
				}
				Board[y][x].EnPassantTarget = false;
				Board[y][x].CastlingTarget = false;
				Board[y][x].Piece = PieceType::NoPiece;
				Board[y][x].Owner = PlayerType::Nobody;
		}
	}

	// Populate the board with pieces
	for (int o : {PlayerType::Black, PlayerType::White}) {
		int x = 0;
		for (int p : {PieceType::Rook,
		              PieceType::Knight,
		              PieceType::Bishop,
		              PieceType::Dame,
		              PieceType::King,
		              PieceType::Bishop,
		              PieceType::Knight,
		              PieceType::Rook}) {

			int y = 0;

			// Rows 0 and 7
			y = (o - 1) * 7;
			Board[y][x].Owner = o;
			Board[y][x].Piece = p;
			
			// Rows 1 and 6
			y = (o - 1) * 7 + 1 - (o - 1) * 2;
			Board[y][x].Owner = o;
			Board[y][x].Piece = PieceType::Pawn;

			x++;
		}
	}

	UpdateCastlingInformation(Turn);

	return;
}

bool AlderChess::GameLogic::Save() {
	// Saves the game state. Returns the status of success.

	const int FileBufferSize = 4 + 1 + 8 * 8;
	unsigned char FileBuffer[FileBufferSize] = { 0 };

	// Header
	FileBuffer[0] = 'A';
	FileBuffer[1] = 'L';
	FileBuffer[2] = 'D';
	FileBuffer[3] = '1';

	// Turn
	FileBuffer[4] = Turn;

	/* Each following byte represents represents the state
	   of a particular square as follows:

	   (MSB)
	   Bit 7     (unused)
	   Bit 6     En passant target
	   Bit 5     Touched
	   Bit 2..4  Piece type
	   Bit 0..1  Owner
	   (LSB)
	*/

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			int OffsetY = 4 + 1 + y * 8;
			FileBuffer[OffsetY + x] = ((unsigned char)Board[y][x].Owner << 0) |
			                          ((unsigned char)Board[y][x].Piece << 2) |
			                          ((unsigned char)Board[y][x].Touched << 5) |
			                          ((unsigned char)Board[y][x].EnPassantTarget << 6);
		}
	}

	char *SDLBasePath = SDL_GetPrefPath("Alder Chess", "Alder Chess");
	std::string FileName(SDLBasePath);
	FileName += "saved_game.alderchess";
	SDL_free(SDLBasePath);

	try {
		std::ofstream FileHandle(FileName, ios::binary);
		FileHandle.write((const char *)FileBuffer, FileBufferSize);
		FileHandle.close();
	} catch(...) {
		return false;
	}

	return true;
}

bool AlderChess::GameLogic::Load() {
	// Loads the game state. Returns the status of success.

	const int FileBufferSize = 4 + 1 + 8 * 8;
	unsigned char FileBuffer[FileBufferSize] = { 0 };

	char *SDLBasePath = SDL_GetPrefPath("Alder Chess", "Alder Chess");
	std::string FileName(SDLBasePath);
	FileName += "saved_game.alderchess";
	SDL_free(SDLBasePath);

	try {
		std::ifstream FileHandle(FileName, ios::binary | ios::ate);
		
		if (FileHandle.tellg() < (FileBufferSize - 1)) throw -1;
		
		FileHandle.seekg(0);
		FileHandle.read((char *)FileBuffer, FileBufferSize);

		bool GoodHeader = true;

		GoodHeader &= (FileBuffer[0] == 'A');
		GoodHeader &= (FileBuffer[1] == 'L');
		GoodHeader &= (FileBuffer[2] == 'D');
		GoodHeader &= (FileBuffer[3] == '1');

		if (!GoodHeader) throw -1;

		Promotion.Pending = false;
		Promotion.Location = {0, 0};

		Turn = FileBuffer[4];

		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				int OffsetY = 4 + 1 + y * 8;
				Board[y][x].Owner           = (FileBuffer[OffsetY + x] & 0x03) >> 0;
				Board[y][x].Piece           = (FileBuffer[OffsetY + x] & 0x1C) >> 2;
				Board[y][x].Touched         = (FileBuffer[OffsetY + x] & 0x20) >> 5;
				Board[y][x].EnPassantTarget = (FileBuffer[OffsetY + x] & 0x40) >> 6;
			}
		}

		FileHandle.close();
	} catch(...) {
		return false;
	}

	UpdateCastlingInformation(Turn);

	return true;
}

void AlderChess::GameLogic::UpdateCastlingInformation(int Player) {
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			Board[y][x].CastlingTarget = false;
		}
	}

	if (Player == PlayerType::Nobody) return;

	// King cannot be in check
	if (KingInCheck(Player)) return;

	int Row = 0;

	if (Player == PlayerType::White) {
		Row = 7;
	}

	XY KingLocation = FindKing(Player);
	if (!KingLocation.InRange8()) return;

	// The king must not have moved
	if ((KingLocation.x != 4) &&
	    (KingLocation.y != Row) &&
		Board[KingLocation.y][KingLocation.x].Touched != false) {
			return;
	}

	Matrix<int, 8, 8> TravelMapOpponentGlobal = EmptyMatrix8x8();
	TravelMapOpponentGlobal = GetTravelMapGlobal(Opponent(Player), true, true, true);

	for (int x : {0, 7}) {
		// There must be a rook that has not moved
		if ((Board[Row][x].Owner == Player) &&
		    (Board[Row][x].Piece == PieceType::Rook) &&
		    (Board[Row][x].Touched == false)) {
			XY RookLocation(x, Row);

			bool EmptyGround = true;
			int StartX = 0;
			int EndX = 0;

			if (RookLocation.x < KingLocation.x) {
				StartX = RookLocation.x;
				EndX = KingLocation.x;
			} else {
				StartX = KingLocation.x;
				EndX = RookLocation.x;
			}

			if (x == 0) {
				/* The squares between king and the rook have to be empty
				   and not under attack. */
				for (int e = (StartX + 1); e < EndX; e++) {
					if ((Board[Row][e].Owner != PlayerType::Nobody) ||
					    (TravelMapOpponentGlobal[Row][e] > 0)) {
						EmptyGround = false;
					}
				}

				if (EmptyGround == true) {
					Board[Row][2].CastlingTarget = true;
				}
			} else if (x == 7) {
				/* The squares between king and the rook have to be empty
				   and not under attack. */
				for (int e = (StartX + 1); e < EndX; e++) {
					if ((Board[Row][e].Owner != PlayerType::Nobody) ||
					    (TravelMapOpponentGlobal[Row][e] > 0)) {
						EmptyGround = false;
					}
				}

				if (EmptyGround == true) {
					Board[Row][6].CastlingTarget = true;
				}
			}
		}
	}

	return;
}

bool AlderChess::GameLogic::CanSelect(int Player, XY SquareCoordinates) {
	/* This function returns true if the player can select
	   the specified piece, otherwise false. */

	if (Board[SquareCoordinates.y][SquareCoordinates.x].Owner == Player) return true;

	return false;
}

Matrix<int, 8, 8> AlderChess::GameLogic::GetTravelMap(int Player,
                                                      XY SquareCoordinates,
                                                      bool SubstractKingIllegal,
                                                      bool IncludeSpecialMoves,
                                                      bool PreventRecursiveKingInCheck,
                                                      bool IncludeKing,
                                                      bool AlwaysIncludePawnDiagonal) {
	// Get allowed moves for a piece in a certain square

	Matrix<int, 8, 8> TravelMap = EmptyMatrix8x8();

	if (!CanSelect(Player, SquareCoordinates)) return TravelMap;
	
	std::vector<XY> SpeedVectors;
	int Distance = 0;

	switch (Board[SquareCoordinates.y][SquareCoordinates.x].Piece) {
		case PieceType::Pawn:
			{
				Distance = 1;

				int dy = 0;

				if (Player == PlayerType::Black) {
					dy = 1;
				} else {
					dy = -1;
				}

				// Check free space moving possibilities

				XY TestFreeSpace(SquareCoordinates.x, SquareCoordinates.y);

				TestFreeSpace.y += dy;
				if (TestFreeSpace.InRange8() &&
					(Board[TestFreeSpace.y][TestFreeSpace.x].Owner == PlayerType::Nobody)) {
					SpeedVectors.push_back({0, dy});

					TestFreeSpace.y += dy;
					if (TestFreeSpace.InRange8() &&
						(Board[TestFreeSpace.y][TestFreeSpace.x].Owner == PlayerType::Nobody) &&
						!Board[SquareCoordinates.y][SquareCoordinates.x].Touched) {
						// Pawn can move two steps from its initial position
						SpeedVectors.push_back({0, 2 * dy});
					}
				}

				// Check diagonal attack possibilities

				XY TestDiagonalAttack(SquareCoordinates.x, SquareCoordinates.y);

				TestDiagonalAttack.y += dy;

				for (int dx : {-1, 1}) {
					TestDiagonalAttack.x = SquareCoordinates.x + dx;

					/* AlwaysIncludePawnDiagonal is used to check potential attack opportunities
					   against enemy king, even if the enemy king is currently not in the target
					   square. */

					if ((TestDiagonalAttack.InRange8() &&
						(Board[TestDiagonalAttack.y][TestDiagonalAttack.x].Owner == Opponent(Player)))
						|| AlwaysIncludePawnDiagonal) {
						SpeedVectors.push_back({dx, dy});
					}
				}

				if (IncludeSpecialMoves) {
					XY TestEnPassant;

					for (int dx : {-1, 1}) {
						// See if there is an En passant opportunity for pawn

						TestEnPassant.x = SquareCoordinates.x + dx;
						TestEnPassant.y = SquareCoordinates.y + dy;

						if (TestEnPassant.InRange8()) {
								if (Board[TestEnPassant.y]
										 [TestEnPassant.x].EnPassantTarget == true) {
									SpeedVectors.push_back({dx, dy});
								}
						}
					}
				}
			}
			break;

		case PieceType::Knight:
			Distance = 1;
			for (int y = -2; y <= 2; y++) {
				for (int x = -2; x <= 2; x++) {
					if (x != 0 && y != 0 && std::abs(x) != std::abs(y)) {
						SpeedVectors.push_back({x, y});
					}
				}
			}
			break;

		case PieceType::Bishop:
			Distance = -1;
			for (int y = -1; y <= 1; y++) {
				for (int x = -1; x <= 1; x++) {
					if (x != 0 && y != 0) {
						SpeedVectors.push_back({x, y});
					}
				}
			}
			break;

		case PieceType::Rook:
			Distance = -1;
			for (int y = -1; y <= 1; y++) {
				for (int x = -1; x <= 1; x++) {
					if (x != y && (x == 0 || y == 0)) {
						SpeedVectors.push_back({x, y});
					}
				}
			}
			break;

		case PieceType::Dame:
			Distance = -1;
			for (int y = -1; y <= 1; y++) {
				for (int x = -1; x <= 1; x++) {
					if (!(x == 0 && y == 0)) {
						SpeedVectors.push_back({x, y});
					}
				}
			}
			break;

		case PieceType::King:
			if (IncludeKing) {
				Distance = 1;
				for (int y = -1; y <= 1; y++) {
					for (int x = -1; x <= 1; x++) {
						if (!(x == 0 && y == 0)) {
							SpeedVectors.push_back({x, y});
						}
					}
				}

				if (IncludeSpecialMoves) {
					// Find castling targets
					for (int y = 0; y < 8; y++) {
						for (int x = 0; x < 8; x++) {
							if (Board[y][x].CastlingTarget == true) {
								TravelMap[y][x] = 1;
							}
						}
					}
				}
			}
			break;
	}

	if (!SpeedVectors.empty()) {
		for (XY sv : SpeedVectors) {
			TravelMap = TravelMap +
			            Travel(Board[SquareCoordinates.y]
			                        [SquareCoordinates.x].Owner,
			                   SquareCoordinates,
			                   sv,
			                   Distance);
		}
	}

	TravelMap = NormalizeMatrix8x8(TravelMap);

	if (Board[SquareCoordinates.y][SquareCoordinates.x].Piece == PieceType::King &&
	    SubstractKingIllegal) {
		/* The king is not allowed to move to squares where it would be under
		   attack by the opponent. */

		/* There are some cases where we don't want to substract the attacking
		   squares from the move targets of the king. For those cases the caller
		   sets the boolean SubstractKingIllegal to false. It is also is done by
		   GetTravelMapGlobal() to prevent an infinite call chain through
		   GetTravelMap() and GetTravelMapGlobal(). */

		Matrix<int, 8, 8> TravelMapOpponentGlobal = EmptyMatrix8x8();
		TravelMapOpponentGlobal = GetTravelMapGlobal(Opponent(Player), true, true, true);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				TravelMap[y][x] = TravelMap[y][x] - TravelMapOpponentGlobal[y][x];
			}
		}
	}

	/* If the king is in check, the player is not allowed to make any move
	   that would not get the king out of check. So remove any such locations
	   from the travel map of this piece. */

	/* PreventRecursiveKingInCheck used to prevent an infinite call chain
	   through KingInCheck() -> GetTravelMapGlobal() -> KingInCheck()... */

	if (!PreventRecursiveKingInCheck) {
		if (KingInCheck(Player)) {
			for (int y = 0; y < 8; y++) {
				for (int x = 0; x < 8; x++) {
					if (TravelMap[y][x] > 0) {
						// Would moving this piece get the king out of check?
						SwapPieces(SquareCoordinates, {x, y});
						if (KingInCheck(Player)) {
							// Still in check, remove from map
							TravelMap[y][x] = 0;
						}
						SwapPieces(SquareCoordinates, {x, y});
					}
				}
			}
		}
	}

	return NormalizeMatrix8x8(TravelMap);
}

Matrix<int, 8, 8> AlderChess::GameLogic::GetTravelMapGlobal(
		int Player,
		bool PreventRecursiveKingInCheck,
		bool IncludeKing,
        bool AlwaysIncludePawnDiagonal) {

	// Get combined travel map for all pieces of a player

	Matrix<int, 8, 8> TravelMap = EmptyMatrix8x8();

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			if (Board[y][x].Owner == Player) {
				TravelMap = TravelMap +
				            GetTravelMap(Player, {x, y},
				            false,
				            false,
				            PreventRecursiveKingInCheck,
				            IncludeKing,
                            AlwaysIncludePawnDiagonal);
			}
		}
	}

	return NormalizeMatrix8x8(TravelMap);
}

Matrix<int, 8, 8> AlderChess::GameLogic::Travel(int Player,
                                                XY Location,
                                                XY SpeedVector,
                                                int Distance) {

	/* Travels the piece as far as possible in the specified direction
	   and builds the travel map along the way. */

	// Distance == -1 means unlimited distance

	Matrix<int, 8, 8> TravelMap = EmptyMatrix8x8();

	for (;;) {
		bool KeepTravelling = true;
		
		KeepTravelling &= (Board[Location.y][Location.x].Owner != Opponent(Player));
		KeepTravelling &= (Distance == -1 || Distance-- > 0);

		Location += SpeedVector;

		KeepTravelling &= Location.InRange8();
		KeepTravelling &= (Board[Location.y][Location.x].Owner != Player);

		if (KeepTravelling) {
			TravelMap[Location.y][Location.x] = 1;
		} else {
			break;
		}
	}

	return TravelMap;
}

AlderChess::XY AlderChess::GameLogic::FindKing(int Player) {
	/* Finds the location of the king of a player.
	   Returns {-1, -1} if king is not found (should never happen). */

	XY KingLocation(-1, -1);

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			if (Board[y][x].Owner == Player && Board[y][x].Piece == PieceType::King) {
				KingLocation.x = x;
				KingLocation.y = y;
			}
		}
	}

	return KingLocation;
}

bool AlderChess::GameLogic::KingInCheck(int Player) {
	// Check if the king is in check for a player

	bool KingCheck = false;
	XY KingLocation(FindKing(Player));
	if (!KingLocation.InRange8()) return false;

	Matrix<int, 8, 8> TravelMapOpponentGlobal = EmptyMatrix8x8();
	TravelMapOpponentGlobal = GetTravelMapGlobal(Opponent(Player), true, true, true);

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			if (TravelMapOpponentGlobal[y][x] > 0 &&
			    x == KingLocation.x &&
			    y == KingLocation.y) {
					KingCheck = true;
			}
		}
	}

	return KingCheck;
}

bool AlderChess::GameLogic::PromotionRequested() {
	return Promotion.Pending;
}

AlderChess::XY AlderChess::GameLogic::QueryPromotionLocation() {
	if (!Promotion.Pending) return {-1, -1};
	return Promotion.Location;
}

void AlderChess::GameLogic::SatisfyPromotionRequest(int Piece) {
	Board[Promotion.Location.y][Promotion.Location.x].Piece = Piece;
	Promotion.Pending = false;
	Promotion.Location = {-1, -1};

	return;
}

bool AlderChess::GameLogic::PlayerCanMove(int Player) {
	// Check if a player can make any move

	Matrix<int, 8, 8> TravelMapGlobal = EmptyMatrix8x8();
	TravelMapGlobal = GetTravelMapGlobal(Player, false, false, false);
	TravelMapGlobal = TravelMapGlobal + GetTravelMap(Player, FindKing(Player),
	                                                 true, true, false, true, false);

	bool CanMove = false;

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			if (TravelMapGlobal[y][x] > 0) {
				CanMove = true;
			}
		}
	}

	return CanMove;
}

bool AlderChess::GameLogic::Checkmate(int Player) {
	// Report whether there is a checkmate condition for a player

	/* Checkmate is defined as the king of the player being in check
	   and there is no legal move to get the king out of check. It
	   results into a victory for the opponent. */

	if (KingInCheck(Player) && !PlayerCanMove(Player)) {
		return true;
	}

	return false;
}

bool AlderChess::GameLogic::Stalemate(int Player) {
	// Report whether there is a stalemate condition for a player

	/* Stalemate is defined as the king of the player not being in check
	   but there is no legal move to make. It results into a draw. */

	if (!KingInCheck(Player) && !PlayerCanMove(Player)) {
		return true;
	}

	return false;
}

int AlderChess::GameLogic::Opponent(int Player) {
	if (Player == PlayerType::Black) return PlayerType::White;
	if (Player == PlayerType::White) return PlayerType::Black;
	return PlayerType::Nobody;
}

int AlderChess::GameLogic::GetTurn() {
	return Turn;
}

void AlderChess::GameLogic::ChangeTurn() {
	Turn = Opponent(Turn);
	UpdateCastlingInformation(Turn);
	return;
}

void AlderChess::GameLogic::SwapPieces(XY Source, XY Destination) {
	/* Swap two pieces. This function is used to test whether moving a certain
	   piece would result in a game state where the king is no longer in check. */

	int TempOwner = Board[Destination.y][Destination.x].Owner;
	int TempPiece = Board[Destination.y][Destination.x].Piece;
	bool TempTouched = Board[Destination.y][Destination.x].Touched;

	Board[Destination.y][Destination.x].Owner = Board[Source.y][Source.x].Owner;
	Board[Destination.y][Destination.x].Piece = Board[Source.y][Source.x].Piece;
	Board[Destination.y][Destination.x].Touched = Board[Source.y][Source.x].Touched;

	Board[Source.y][Source.x].Owner = TempOwner;
	Board[Source.y][Source.x].Piece = TempPiece;
	Board[Source.y][Source.x].Touched = TempTouched;

	return;
}

bool AlderChess::GameLogic::Move(XY Source, XY Destination) {
	/* Move a piece from source to destination.
	   Returns true or false depending on success. */

	if (!CanSelect(Board[Source.y][Source.x].Owner, Source)) return false;

	Matrix<int, 8, 8> TravelMap = GetTravelMap(Board[Source.y][Source.x].Owner,
	                                           Source,
	                                           true, true, false, true, false);

	if (TravelMap[Destination.y][Destination.x] > 0) {
		// Move the piece
		Board[Destination.y][Destination.x].Owner = Board[Source.y][Source.x].Owner;
		Board[Destination.y][Destination.x].Piece = Board[Source.y][Source.x].Piece;
		Board[Source.y][Source.x].Owner = PlayerType::Nobody;
		Board[Source.y][Source.x].Piece = PieceType::NoPiece;
		Board[Source.y][Source.x].Touched = true;

		// Check if castling was performed
		if ((Board[Destination.y][Destination.x].Piece == PieceType::King) &&
		    (Board[Destination.y][Destination.x].CastlingTarget == true)) {
			// Find out which rook is in the direction of castle
			XY RookSource(0, Destination.y);
			XY RookDestination(3, Destination.y);
			if (Destination.x > Source.x) {
				RookSource.x = 7;
				RookDestination.x = 5;
			}

			// Move the associated rook automatically
			Board[RookDestination.y][RookDestination.x].Owner = Board[RookSource.y][RookSource.x].Owner;
			Board[RookDestination.y][RookDestination.x].Piece = Board[RookSource.y][RookSource.x].Piece;
			Board[RookSource.y][RookSource.x].Owner = PlayerType::Nobody;
			Board[RookSource.y][RookSource.x].Piece = PieceType::NoPiece;
			Board[RookSource.y][RookSource.x].Touched = true;
		}

		// Check if En passant attack was performed
		if ((Board[Destination.y][Destination.x].Piece == PieceType::Pawn) &&
		    (Board[Destination.y][Destination.x].EnPassantTarget == true)) {
			XY CapturePiece(Destination.x, Destination.y);

			if (Turn == PlayerType::Black) {
				CapturePiece.y--;
			} else if (Turn == PlayerType::White) {
				CapturePiece.y++;
			}

			if (CapturePiece.InRange8()) {
				Board[CapturePiece.y][CapturePiece.x].Owner = PlayerType::Nobody;
				Board[CapturePiece.y][CapturePiece.x].Piece = PieceType::NoPiece;
			}
		}

		// Clear En passant information
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				Board[y][x].EnPassantTarget = false;
			}
		}

		// Update En passant target for pawn
		if ((Board[Destination.y][Destination.x].Piece == PieceType::Pawn) &&
			(std::abs(Destination.y - Source.y) == 2)) {

			// The pawn moved two steps, so En passant location is marked behind it
			if (Board[Destination.y][Destination.x].Owner == PlayerType::Black) {

				// Black pawn moved south
				Board[Destination.y - 1][Destination.x].EnPassantTarget = true;

			} else if (Board[Destination.y][Destination.x].Owner == PlayerType::White) {

				// White pawn moved north
				Board[Destination.y + 1][Destination.x].EnPassantTarget = true;

			}
		}

		// See if pawn should be promoted
		if (Board[Destination.y][Destination.x].Piece == PieceType::Pawn) {
			if (((Board[Destination.y][Destination.x].Owner == PlayerType::Black) &&
			    (Destination.y == 7)) ||
				((Board[Destination.y][Destination.x].Owner == PlayerType::White) &&
			    (Destination.y == 0))) {
				/* Either a black pawn has moved to the lower row or a white pawn
				   has moved to the upper row. */

				// The renderer must handle the promotion request immediately

				Promotion.Pending = true;
				Promotion.Location = {Destination.x, Destination.y};
			}
		}

		return true;
	}

	return false;
}

int AlderChess::GameLogic::GetPiece(XY Coordinates) {
	return Board[Coordinates.y][Coordinates.x].Piece;
}

int AlderChess::GameLogic::GetOwner(XY Coordinates) {
	return Board[Coordinates.y][Coordinates.x].Owner;
}

void AlderChess::GameLogic::SetPieceDebug(XY Coordinates, int Piece) {
	Board[Coordinates.y][Coordinates.x].Piece = Piece;
	UpdateCastlingInformation(Turn);
}

void AlderChess::GameLogic::SetOwnerDebug(XY Coordinates, int Owner) {
	Board[Coordinates.y][Coordinates.x].Owner = Owner;
	UpdateCastlingInformation(Turn);
}