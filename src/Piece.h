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

typedef int Piece;

const Piece Pawn = 0;
const Piece Knight = 1;
const Piece Bishop = 2;
const Piece Rook = 3;
const Piece Queen = 4;
const Piece King = 5;
const Piece NoPiece = 6;

const static char piece_notation[] = " nbrqk"; 

const char* pieceToString(int piece, char* buf) {
	sprintf(buf, "%c", piece_notation[piece]);
	return buf;
}

typedef int Side;
