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

typedef int Move;

// to do: make inlined functions
#define PIECE(x) ((x >> 26) & 15)
#define SETPIECE(x,y) (x |= (y) << 26)
#define CAPTURED(x) ((x >> 22) & 15)
#define SETCAPTURED(x,y) (x |= (y) << 22)
#define PROMOTED(x) ((x >> 18) & 15)
#define SETPROMOTED(x,y) (x |= (y) << 18)
#define TYPE(x) ((x >> 12) & 63)
#define SETTYPE(x,y) (x |= (y) << 12)
#define FROM(x) ((x >> 6) & 63)
#define SETFROM(x,y) (x |= (y) << 6)
#define TO(x) (x & 63)
#define SETTO(x,y) (x |= (y))

__forceinline Side side(const Move& m) {
	return (m >> 29) & 1;
}

__forceinline int sideMask(const Move& m) {
	return side(m) << 3;
}

const int QUIET =  1;
const int DOUBLEPUSH =  2;
const int CASTLE = 4;
const int EPCAPTURE = 8;
const int PROMOTION = 16;
const int CAPTURE = 32;
const int ALLMOVES = 63;

__forceinline int isCapture(const Move& m) { 
	return TYPE(m) & (CAPTURE | EPCAPTURE); 
}

__forceinline int isEpCapture(const Move& m) { 
	return TYPE(m) & EPCAPTURE; 
}

__forceinline int isCastleMove(const Move& m) { 
	return TYPE(m) & CASTLE; 
}

__forceinline int isPromotion(const Move& m) { 
	return TYPE(m) & PROMOTION; 
}

__forceinline int isQueenPromotion(const Move& m) { 
	return isPromotion(m) && (PROMOTED(m) & 7) == Queen; 
}

__forceinline void initMove(Move& move, const Piece piece, 
	const Piece captured, const Square from, const Square to, 
	const int type, const Piece promoted) 
{
	move = 0;
	SETPIECE(move, piece);
	SETCAPTURED(move, captured);
	SETPROMOTED(move, promoted);
	SETFROM(move, from);
	SETTO(move, to);
	SETTYPE(move, type);
}

const char* moveToString(const Move m, char* buf) {
	char tmp1[12], tmp2[12], tmp3[12];
	sprintf(buf, "%s%s", squareToString(FROM(m), tmp1), squareToString(TO(m), tmp2));
	if (isPromotion(m)) {
		sprintf(&buf[strlen(buf)], "%s", piece_str(PROMOTED(m) & 7, tmp3));
	}
	return buf;
}
