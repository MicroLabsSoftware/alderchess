/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#include "stdafx.hpp"
#include "alderchess.hpp"

int main(int argc, char* argv[]) {
	AlderChess::EventManager Event;

	if (!Event.good()) return -1;

	Event.Run();

	return 0;
}