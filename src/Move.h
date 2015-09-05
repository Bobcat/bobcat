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

typedef uint32_t Move;

__forceinline int movePiece(const Move move) { return (move >> 26) & 15; }
__forceinline void moveSetPiece(Move& move, const int piece) { move |= (piece << 26); }
__forceinline int moveCaptured(const Move move) { return (move >> 22) & 15; }
__forceinline void moveSetCaptured(Move& move, const int piece) { move |= (piece << 22); }
__forceinline int movePromoted(const Move move) { return (move >> 18) & 15; }
__forceinline void moveSetPromoted(Move& move, const int piece) { move |= (piece << 18); }
__forceinline int moveType(const Move move) { return (move >> 12) & 63; }
__forceinline void moveSetType(Move& move, const int type) { move |= (type << 12); }
__forceinline uint32_t moveFrom(const Move move) { return (move >> 6) & 63; }
__forceinline void moveSetFrom(Move& move, const int sq) { move |= (sq << 6); }
__forceinline uint32_t moveTo(const Move move) { return move & 63; }
__forceinline void moveSetTo(Move& move, const int sq) { move |= sq; }
__forceinline int movePieceType(const Move move) { return (move >> 26) & 7; }

__forceinline Side moveSide(const Move m) {
	return (m >> 29) & 1;
}

__forceinline int sideMask(const Move m) {
	return moveSide(m) << 3;
}

const int QUIET =  1;
const int DOUBLEPUSH =  2;
const int CASTLE = 4;
const int EPCAPTURE = 8;
const int PROMOTION = 16;
const int CAPTURE = 32;

__forceinline int isCapture(const Move m) {
	return moveType(m) & (CAPTURE | EPCAPTURE);
}

__forceinline int isEpCapture(const Move m) {
	return moveType(m) & EPCAPTURE;
}

__forceinline int isCastleMove(const Move m) {
	return moveType(m) & CASTLE;
}

__forceinline int isPromotion(const Move m) {
	return moveType(m) & PROMOTION;
}

__forceinline bool isQueenPromotion(const Move m) {
	return isPromotion(m) && (movePromoted(m) & 7) == Queen;
}

__forceinline bool isNullMove(const Move m) {
	return m == 0;
}

__forceinline void initMove(Move& move, const Piece piece,
	const Piece captured, const Square from, const Square to,
	const int type, const Piece promoted)
{
	move = 0;
	moveSetPiece(move, piece);
	moveSetCaptured(move, captured);
	moveSetPromoted(move, promoted);
	moveSetFrom(move, from);
	moveSetTo(move, to);
	moveSetType(move, type);
}
