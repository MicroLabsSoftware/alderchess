/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#pragma once

#include <memory>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#ifdef _WIN32
	#define NOMINMAX
	#include <Windows.h>
#endif
#include <SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <SDL.h>
#include "matrix.hpp"