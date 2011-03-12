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

class Game {
public:
	__forceinline bool makeMove(const Move m, bool check_legal = true, bool calculate_in_check = true) { 
		if (m == 0) {
			return MakeNullMove();
		}
		board.makeMove(m);
		if (check_legal && !(TYPE(m) & CASTLE)) { 
			if (board.isAttacked(board.king_square[pos->side_to_move], pos->side_to_move ^ 1)) {
				board.unmakeMove(m);
				return false;
			}
		}
		Position* prev = pos++;
		pos->side_to_move = prev->side_to_move ^ 1;
		pos->material = prev->material;
		pos->last_move = m; 
		if (calculate_in_check) {
			pos->in_check = board.isAttacked(board.king_square[pos->side_to_move], pos->side_to_move ^ 1);
		}
		pos->castle_rights = prev->castle_rights & castle_rights_mask[FROM(m)] & castle_rights_mask[TO(m)]; 
		pos->null_search = prev->null_search;
		pos->null_moves_in_row = 0;
		if (isCapture(m) || (PIECE(m) & 7) == Pawn) {
			pos->reversible_half_move_count = 0;
		}
		else {
			pos->reversible_half_move_count = prev->reversible_half_move_count + 1;
		}
		if (TYPE(m) & DOUBLEPUSH) {
			pos->en_passant_square = bb_square(TO(m) + pawn_push_dist[pos->side_to_move]);
		} 
		else {
			pos->en_passant_square = 0;
		}
		pos->key = prev->key;
		pos->pawn_structure_key = prev->pawn_structure_key;
		updateKey(m);
		pos->material.makeMove(m);
		return true;
	}

	__forceinline void unmakeMove() {
		if (pos->last_move) {
			board.unmakeMove(pos->last_move);
		}
		pos--;
	}

	__forceinline bool MakeNullMove() { 
		Position* prev = pos++;
		pos->side_to_move = prev->side_to_move ^ 1;
		pos->material = prev->material;
		pos->last_move = 0; 
		pos->in_check = false;
		pos->castle_rights = prev->castle_rights;
		pos->null_search = true;
		pos->null_moves_in_row = prev->null_moves_in_row + 1;
		pos->reversible_half_move_count = 0;
		pos->en_passant_square = 0;
		pos->key = prev->key;
		pos->pawn_structure_key = prev->pawn_structure_key;
		updateKey(0);
		return true;
	}

	uint64 calculateKey() {
		uint64 key = 0;
		for (int piece = Pawn; piece <= King; piece++) {
			for (int side = 0; side <= 1; side++) {
				for (BB bb = board.piece[piece | (side << 3)]; bb != 0; reset_lsb(bb)) {
					key ^= zobrist_pcsq[piece | (side << 3)][lsb(bb)];
				}
			}
		}
		key ^= zobrist_castling[pos->castle_rights];
		if (pos->en_passant_square) {
			key ^= zobrist_ep_file[file(lsb(pos->en_passant_square))];
		}
		if (pos->side_to_move == 1) {
			key ^= zobrist_side;
		}
		return key;
	}

	__forceinline void updateKey(const Move m) {
		pos->key ^= pos->pawn_structure_key;
		pos->pawn_structure_key ^= zobrist_side;

		if ((pos - 1)->en_passant_square) {
			pos->key ^= zobrist_ep_file[file(lsb((pos - 1)->en_passant_square))];
		}
		if (pos->en_passant_square) {
			pos->key ^= zobrist_ep_file[file(lsb(pos->en_passant_square))];
		}
		if (!m) {
			pos->key ^= pos->pawn_structure_key;
			return;
		}
		// from and to for moving piece
		if ((PIECE(m) & 7) == Pawn) {
			pos->pawn_structure_key ^= zobrist_pcsq[PIECE(m)][FROM(m)];
		}
		else {
			pos->key ^= zobrist_pcsq[PIECE(m)][FROM(m)];
		}
		if (TYPE(m) & PROMOTION) {
			pos->key ^= zobrist_pcsq[PROMOTED(m)][TO(m)];
		}
		else { 
			if ((PIECE(m) & 7) == Pawn) {
				pos->pawn_structure_key ^= zobrist_pcsq[PIECE(m)][TO(m)];
			}
			else {
				pos->key ^= zobrist_pcsq[PIECE(m)][TO(m)];
			}
		}
		// remove captured piece
		if (isEpCapture(m)) {
			pos->pawn_structure_key ^= zobrist_pcsq[CAPTURED(m)][TO(m) + (pos->side_to_move == 1 ? -8 : 8)];
		}
		else if (isCapture(m)) {
			if ((CAPTURED(m) & 7) == Pawn) {
				pos->pawn_structure_key ^= zobrist_pcsq[CAPTURED(m)][TO(m)];
			}
			else {
				pos->key ^= zobrist_pcsq[CAPTURED(m)][TO(m)];
			}
		}
		// castling rights
		if ((pos-1)->castle_rights != pos->castle_rights) {
			pos->key ^= zobrist_castling[(pos-1)->castle_rights];
			pos->key ^= zobrist_castling[pos->castle_rights];
		}
		// rook move in castle
		if (isCastleMove(m)) {
			Piece piece = Rook + sideMask(m);
			pos->key ^= zobrist_pcsq[piece][rook_castles_from[TO(m)]];
			pos->key ^= zobrist_pcsq[piece][rook_castles_to[TO(m)]];
		}
		pos->key ^= pos->pawn_structure_key;
	}

	__forceinline  bool isRepetition() {
		int num_moves = pos->reversible_half_move_count;
		Position* prev = pos;
		while (((num_moves = num_moves - 2) >= 0) && ((prev - position_list) > 1)) {
			prev -= 2;
			if (prev->key == pos->key) {
				return true;
			}
		}
		return false;
	}

	void addPiece(const Piece p, const Side c, const Square sq) {
		int pc = p | (c << 3);
		board.addPiece(p, c, sq);
		pos->key ^= zobrist_pcsq[pc][sq];
		if (p == Pawn) {
			pos->pawn_structure_key ^= zobrist_pcsq[pc][sq];
		}
		pos->material.add(pc);
	}

	int newGame(const char* fen) {
		if (setFen(fen) == 0) {
			return 0;
		}
		return setFen(START_POSITION);
	}
	
	int setFen(const char* fen) {
		pos = position_list;
		pos->clear();
		board.clear();

		const char* p = fen;
		char f = 1;
		char r = 8;

		while (*p != 0 && !(f == 9 && r == 1)) {
			if (isdigit(*p)) {
				f += *p - '0';
				if (f > 9) {
					return 1;
				}
				p++;
				continue;
			}
			if (*p == '/') {
				if (f != 9) {
					return 2;
					break;
				}
				r--;
				f = 1;
				p++;
				continue;
			}
			Side color = 0;
			if (islower(*p)) {
				color = 1;
			}
			Piece piece;
			switch (toupper(*p)) {
				case 'R':
					piece = Rook;
					break;
				case 'N':
					piece = Knight;
					break;
				case 'B':
					piece = Bishop;
					break;
				case 'Q':
					piece = Queen;
					break;
				case 'K':
					piece = King;
					break;
				case 'P':
					piece = Pawn;
					break;
				default:
					return 3;
			} 
			addPiece(piece, color, (f - 1) + (r - 1) * 8);
			f++;
			p++;
			continue;
		}
		if (!get_delimiter(&p) || (pos->side_to_move = get_color_to_move(&p)) == -1) {
			return 4;
		}
		p++;
		if (!get_delimiter(&p) || (pos->castle_rights = get_castle_rights(&p)) == -1) {
			return 5;
		}
		Square sq;
		if (!get_delimiter(&p) || (sq = get_ep_square(&p)) == -1) {
			return 6;
		}
		pos->en_passant_square = sq < 64 ? bb_square(sq) : 0;
		pos->reversible_half_move_count = 0;

		if (pos->side_to_move == 1) {
			pos->key ^= zobrist_side;
			pos->pawn_structure_key ^= zobrist_side;
		}
		pos->key ^= zobrist_castling[pos->castle_rights];
		if (pos->en_passant_square) {
			pos->key ^= zobrist_ep_file[file(lsb(pos->en_passant_square))];
		}
		pos->in_check = board.isAttacked(board.king_square[pos->side_to_move], pos->side_to_move ^ 1);
		return 0;
	}

	int getFen(char* fen) {
		char piece_letter[] = { "PNBRQK  pnbrqk" };
		fen[0] = 0;

		for (char r = 7; r >= 0; r--) {
			int empty = 0;
			for (char f = 0; f <= 7; f++) {
				Square sq = r * 8 + f;
				Piece p = board.getPiece(sq);
				if (p != NoPiece) {
					if (empty) {
						sprintf(fen + strlen(fen), "%d", empty);
						empty = 0;
					}
					sprintf(fen + strlen(fen), "%c", piece_letter[p]);
				}
				else {
					empty++;
				}
			}
			if (empty) {
				sprintf(fen + strlen(fen), "%d", empty);
			}
			if (r > 0) {
				sprintf(fen + strlen(fen), "/");
			}
		}
		sprintf(fen + strlen(fen), " %c ", pos->side_to_move == 0 ? 'w' : 'b');
		if (pos->castle_rights == 0) {
			sprintf(fen + strlen(fen), "- ");
		}
		else {
			if (pos->castle_rights & 1) {
				sprintf(fen + strlen(fen), "K");
			}
			if (pos->castle_rights & 2) {
				sprintf(fen + strlen(fen), "Q");
			}
			if (pos->castle_rights & 4) {
				sprintf(fen + strlen(fen), "k");
			}
			if (pos->castle_rights & 8) {
				sprintf(fen + strlen(fen), "q");
			}
			sprintf(fen + strlen(fen), " ");
		}
		if (pos->en_passant_square) {
			Square ep = lsb(pos->en_passant_square);
			char buf[12];
			sprintf(fen + strlen(fen), "%s ", squareToString(ep, buf));
		}
		else {
			sprintf(fen + strlen(fen), "- ");
		}
		sprintf(fen + strlen(fen), "%d ", pos->reversible_half_move_count);
		sprintf(fen + strlen(fen), "%d", pos->reversible_half_move_count / 2 + 1);
		return 0;
	}

	const char get_ep_square(const char** p) {
		if (**p == '-') {
			return 64; // 64 = no en passant square
		}
		if (**p < 'a' || **p > 'h') {
			return -1;
		}
		(*p)++;
		if (**p != '3' && **p != '6') {
			return -1;
		}
		return (*((*p) - 1) - 'a') + (**p - '1') * 8;
	}

	int get_castle_rights(const char** p) { 
		int rights = 0;
		if (**p == '-') {
			(*p)++;
			return rights;
		}
		while (**p != 0) {
			if (**p == 'K' && rights < 1) {
				rights |= 1;
			}
			else if (**p == 'Q' && rights < 2) {
				rights |= 2;
			}
			else if (**p == 'k' && rights < 4) {
				rights |= 4;
			}
			else if (**p == 'q' && rights < 8) {
				rights |= 8;
			}
			else {
				break;
			}
			(*p)++;
		}
		return rights == 0 ? -1 : rights;
	}

	char get_color_to_move(const char** p) {
		switch (**p) {
			case 'w':
				return 0;
			case 'b':
				return 1;
			default: 
				break;
		}
		return -1;
	}

	bool get_delimiter(const char** p) {
		if (**p != ' ') {
			return false;
		}
		(*p)++;
		return true;
	}

	Game(Config* config) : config(config) {
		for (int i = 0; i < 2000; i++) {
			position_list[i].board = &board;
		}
		pos = position_list;
	}

	void copy(Game* other) {
		board = other->board;
		pos += (other->pos - other->position_list);
		for (int i = 0; i < 2000; i++) {
			position_list[i] = other->position_list[i];
			position_list[i].board = &board;
		}
	}

public:
	Config* config;
	Position* pos;
	Position position_list[2000];
	Board board;

	static const char START_POSITION[];
};

const char Game::START_POSITION[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
