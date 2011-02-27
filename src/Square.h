/*
  This file is part of Bobcat.
  Copyright 2008-2011 Gunnar Harms

  Bobcat is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Bobcat is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Bobcat.  If not, see <http://www.gnu.org/licenses/>.
*/

typedef int Square;

__forceinline int rank(const Square sq) { 
	return sq >> 3; 
}

__forceinline int file(const Square sq) { 
	return sq & 7; 
} 

__forceinline Square square(const Square f, const Square r) { 
	return (r << 3) + f; 
}

__forceinline bool isDark(const Square sq) {
	return ((9 * sq) & 8) == 0;
}

__forceinline bool sameColor(const Square sq1, const Square sq2) {
	return isDark(sq1) == isDark(sq2);
}

enum enSquare {
  a1, b1, c1, d1, e1, f1, g1, h1,
  a2, b2, c2, d2, e2, f2, g2, h2,
  a3, b3, c3, d3, e3, f3, g3, h3,
  a4, b4, c4, d4, e4, f4, g4, h4,
  a5, b5, c5, d5, e5, f5, g5, h5,
  a6, b6, c6, d6, e6, f6, g6, h6,
  a7, b7, c7, d7, e7, f7, g7, h7,
  a8, b8, c8, d8, e8, f8, g8, h8
};

int oo_allowed_mask[2] = { 1, 4 };
int ooo_allowed_mask[2] = { 2, 8 };
int oo_king_from[2] = { e1, e8 };
int oo_king_to[2] = { g1, g8 };
int ooo_king_from[2] = { e1, e8 };
int ooo_king_to[2] = { c1, c8 };
int rook_castles_to[64];
int rook_castles_from[64];
int castle_rights_mask[64];

uint manhattan_distance[64][64];
uint chebyshev_distance[64][64];
uint flip[2][64];

static void Square_h_initialise() {
	for (int sq = 0; sq < 64; sq++) {
		flip[0][sq] = file(sq) + ((7 - rank(sq)) << 3);
		flip[1][sq] = file(sq) + (rank(sq) << 3);
		castle_rights_mask[sq] = 15;
	}
	for (int sq1 = 0; sq1 < 64; sq1++) {
		for (int sq2 = 0; sq2 < 64; sq2++) {
			uint ranks = abs(rank(sq1) - rank(sq2));
			uint files = abs(file(sq1) - file(sq2));
			manhattan_distance[sq1][sq2] = ranks + files;
			chebyshev_distance[sq1][sq2] = max(ranks, files);
		}
	}
	for (int side = 0; side <= 1; side++) {
		rook_castles_from[flip[side][g1]] = flip[side][h1];
		rook_castles_to[flip[side][g1]] = flip[side][f1];
		rook_castles_from[flip[side][c1]] = flip[side][a1];
		rook_castles_to[flip[side][c1]] = flip[side][d1];
		castle_rights_mask[flip[side ^ 1][a1]] = 15 - ooo_allowed_mask[side];
		castle_rights_mask[flip[side ^ 1][e1]] = 15 - oo_allowed_mask[side] - ooo_allowed_mask[side];
		castle_rights_mask[flip[side ^ 1][h1]] = 15 - oo_allowed_mask[side];
	}
}

const char* squareToString(const Square sq, char* buf) {
	sprintf(buf, "%c%d", (char)(file(sq) + 'a'), rank(sq) + 1);
	return buf;
}
