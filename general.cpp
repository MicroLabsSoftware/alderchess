/* Alder Chess
   (C) Joonas Saarinen
   (C) MicroLabs
   This program is licensed under GNU General Public License (version 3),
   please see the included file gpl-3.0.txt for more details. */

#include "stdafx.hpp"
#include "alderchess.hpp"

void AlderChess::ShowError(std::string Text) {
	/* SDL_ShowSimpleMessageBox() would be a platform-agnostic solution to
	   display an error message. However, a fatal error can occur before SDL
	   is initialized properly, so other solutions are used. */

	#ifdef _WIN32
		MessageBox(NULL, Text.c_str(), "Alder Chess", MB_OK | MB_ICONWARNING);
	#elif
		std::cerr << "Error occurred: " << Text << std::endl;
	#endif

	return;
}

AlderChess::XY::XY(int X, int Y) {
	x = X;
	y = Y;
}

bool AlderChess::XY::operator==(const AlderChess::XY& other) {
	bool same = true;
	if (this != &other) {
		same &= (this->x == other.x);
		same &= (this->y == other.y);
	}
	return same;
}

const AlderChess::XY& AlderChess::XY::operator=(const AlderChess::XY& other) {
	if (this != &other) {
		this->x = other.x;
		this->y = other.y;
	}
	return *this;
}

AlderChess::XY AlderChess::XY::operator+(const AlderChess::XY& other) const {
	XY novel;
	novel.x = this->x + other.x;
	novel.y = this->y + other.y;
	return novel;
}

AlderChess::XY AlderChess::XY::operator-(const AlderChess::XY& other) const {
	XY novel;
	novel.x = this->x - other.x;
	novel.y = this->y - other.y;
	return novel;
}

AlderChess::XY& AlderChess::XY::operator+=(const AlderChess::XY& other) {
	this->x += other.x;
	this->y += other.y;
	return *this;
}

AlderChess::XY& AlderChess::XY::operator-=(const AlderChess::XY& other) {
	this->x -= other.x;
	this->y -= other.y;
	return *this;
}

bool AlderChess::XY::InRange(XY a, XY b) {
	// In range between a and b minus one
	bool inr = true;
	inr &= (x >= a.x);
	inr &= (x < b.x);
	inr &= (y >= a.y);
	inr &= (y < b.y);
	return inr;
}

bool AlderChess::XY::InRange8() {
	/* Because of the chess board, there is a lot of 0..7 comparison in this
	   application, so this is a convenience function for that purpose. */
	return InRange({0, 0}, {8, 8});
}

Matrix<int, 8, 8> AlderChess::EmptyMatrix8x8 () {
	Matrix<int, 8, 8> Empty;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			Empty[y][x] = 0;
		}
	}
	return Empty;
}

Matrix<int, 8, 8> AlderChess::NormalizeMatrix8x8 (Matrix<int, 8, 8> mtx) {
	Matrix<int, 8, 8> NewMatrix;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			mtx[y][x] > 0 ? NewMatrix[y][x] = 1 :
			                NewMatrix[y][x] = 0;
		}
	}
	return NewMatrix;
}