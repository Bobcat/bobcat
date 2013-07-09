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

class Position : public Moves {
public:
	Position() {
		clear();
	}

	void clear() {
		in_check = false;
		castle_rights = 0;
		reversible_half_move_count = 0;
		pawn_structure_key = 0;
		key = 0;
		material.clear();
		last_move = 0;
		null_search = false;
		null_moves_in_row = 0;
		transposition = 0;
		last_move = 0; 
		use_lazy = false;
	}

	const Move* stringToMove(const char* m) {
		int castle_type = -1; // 0 = short, 1 = long

		if (!isCastleMove(m, castle_type) && (m[0] < 'a' || m[0] > 'h' || m[1] < '1' || m[1] > '8' || 
			m[2] < 'a' || m[2] > 'h' || m[3] < '1' || m[3] > '8'))
		{
			return NULL;
		}
		Square from = 0;
		Square to = 0;

		if (castle_type == -1) {
			from = square(m[0] - 'a', m[1] - '1');
			to = square(m[2] - 'a', m[3] - '1');

			// chess 960 - shredder fen
			if ((board->getPiece(from) == King && board->getPiece(to) == Rook) 
				|| (board->getPiece(from) == King + 8 && board->getPiece(to) == Rook + 8))
			{
				castle_type = to > from ? 0 : 1;//ga na
			}
		}

		if (castle_type == 0) {
			from = oo_king_from[side_to_move];
			to = oo_king_to[side_to_move];
		}
		else if (castle_type == 1) {
			from = ooo_king_from[side_to_move];
			to = ooo_king_to[side_to_move];
		}
		generateMoves();

		while (const MoveData* move_data = nextMove()) {
			const Move* move = &move_data->move;

			if (moveFrom(*move) == from && moveTo(*move) == to) {
				if (::isCastleMove(*move) && castle_type == -1) {
					continue;
				}

				if (isPromotion(*move)) {
					if (tolower(m[strlen(m) - 1]) != piece_notation[movePromoted(*move) & 7]) {
						continue;
					}
				}
				return move;
			}
		}
		return NULL;
	}

	bool isCastleMove(const char* m, int& castle_type) {
		if (stricmp(m, "O-O") == 0 || stricmp(m, "OO") == 0 || stricmp(m, "0-0") == 0 || stricmp(m, "00") == 0 
			|| (stricmp(m, "e1g1") == 0 && board->getPieceType(e1) == King) 
			|| (stricmp(m, "e8g8") == 0 && board->getPieceType(e8) == King)) 
		{
			castle_type = 0;
			return true;
		}
		if (stricmp(m, "O-O-O") == 0 || stricmp(m, "OOO") == 0 || stricmp(m, "0-0-0") == 0 || stricmp(m, "000") == 0 
			|| (stricmp(m, "e1c1") == 0 && board->getPieceType(e1) == King) 
			|| (stricmp(m, "e8c8") == 0 && board->getPieceType(e8) == King)) 
		{
			castle_type = 1;
			return true;
		}
		return false;
	}

	__forceinline int isDraw() {
		return flags & RECOGNIZEDDRAW;
	}

	int reversible_half_move_count;
	uint64 pawn_structure_key;
	uint64 key;
	Material material;
	bool null_search;
	int null_moves_in_row;
	int pv_length;
	Move last_move; 
	int eval_score;
	int transp_score;
	int transp_depth;
	int transp_flags;
	bool transp_score_valid;
	bool transp_depth_valid;
	Move transp_move;
	int flags;
	Transposition* transposition;
	bool use_lazy;
};
