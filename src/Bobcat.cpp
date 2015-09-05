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

#include "Env.h"
#include "Pgn.h"
#include "Mersenne.h"
#include "Util.h"
#include "Config.h"
#include "Io.h"
#include "Bitmanip.h"
#include "Square.h"
#include "Bitboard.h"
#include "Magic.h"
#include "Piece.h"
#include "Move.h"
#include "Board.h"
#include "Tables.h"
#include "Material.h"
#include "Moves.h"
#include "Zobrist.h"
#include "Position.h"
#include "Game.h"
#include "See.h"
#include "Eval.h"
#include "Protocol.h"
#include "Search.h"
#include "Book.h"
#include "PgnPlayer.h"
#include "Tune.h"
#include "Test.h"
#include "Worker.h"
#include "Bobcat.h"

int main(int argc, char* argv[]) {
	Bobcat* good_cat = new Bobcat();
	return good_cat->run(argc, argv);
}

