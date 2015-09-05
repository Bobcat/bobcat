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
	Game() : position_list(new Position[2000]), pos(position_list), chess960(false), xfen(false) {
		for (int i = 0; i < 2000; i++) {
			position_list[i].board = &board;
		}
	}

	virtual ~Game() {
		delete[] position_list;
	}

	__forceinline bool makeMove(const Move m, bool check_legal, bool calculate_in_check) {
		if (m == 0) {
			return makeNullMove();
		}
		board.makeMove(m);
		if (check_legal && !(moveType(m) & CASTLE)) {
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
		pos->castle_rights = prev->castle_rights & castle_rights_mask[moveFrom(m)] & castle_rights_mask[moveTo(m)];
		pos->null_moves_in_row = 0;
		if (isCapture(m) || (movePiece(m) & 7) == Pawn) {
			pos->reversible_half_move_count = 0;
		}
		else {
			pos->reversible_half_move_count = prev->reversible_half_move_count + 1;
		}
		if (moveType(m) & DOUBLEPUSH) {
			pos->en_passant_square = bbSquare(moveTo(m) + pawn_push_dist[pos->side_to_move]);
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

	__forceinline bool makeNullMove() {
		Position* prev = pos++;
		pos->side_to_move = prev->side_to_move ^ 1;
		pos->material = prev->material;
		pos->last_move = 0;
		pos->in_check = false;
		pos->castle_rights = prev->castle_rights;
		pos->null_moves_in_row = prev->null_moves_in_row + 1;
		pos->reversible_half_move_count = 0;
		pos->en_passant_square = 0;
		pos->key = prev->key;
		pos->pawn_structure_key = prev->pawn_structure_key;
		updateKey(0);
		return true;
	}

	uint64_t calculateKey() {
		uint64_t key = 0;
		for (int piece = Pawn; piece <= King; piece++) {
			for (int side = 0; side <= 1; side++) {
				for (BB bb = board.piece[piece | (side << 3)]; bb != 0; resetLSB(bb)) {
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
		if ((movePiece(m) & 7) == Pawn) {
			pos->pawn_structure_key ^= zobrist_pcsq[movePiece(m)][moveFrom(m)];
		}
		else {
			pos->key ^= zobrist_pcsq[movePiece(m)][moveFrom(m)];
		}
		if (moveType(m) & PROMOTION) {
			pos->key ^= zobrist_pcsq[movePromoted(m)][moveTo(m)];
		}
		else {
			if ((movePiece(m) & 7) == Pawn) {
				pos->pawn_structure_key ^= zobrist_pcsq[movePiece(m)][moveTo(m)];
			}
			else {
				pos->key ^= zobrist_pcsq[movePiece(m)][moveTo(m)];
			}
		}
		// remove captured piece
		if (isEpCapture(m)) {
			pos->pawn_structure_key ^= zobrist_pcsq[moveCaptured(m)][moveTo(m) + (pos->side_to_move == 1 ? -8 : 8)];
		}
		else if (isCapture(m)) {
			if ((moveCaptured(m) & 7) == Pawn) {
				pos->pawn_structure_key ^= zobrist_pcsq[moveCaptured(m)][moveTo(m)];
			}
			else {
				pos->key ^= zobrist_pcsq[moveCaptured(m)][moveTo(m)];
			}
		}
		// castling rights
		if ((pos - 1)->castle_rights != pos->castle_rights) {
			pos->key ^= zobrist_castling[(pos - 1)->castle_rights];
			pos->key ^= zobrist_castling[pos->castle_rights];
		}
		// rook move in castle
		if (isCastleMove(m)) {
			Piece piece = Rook + sideMask(m);
			pos->key ^= zobrist_pcsq[piece][rook_castles_from[moveTo(m)]];
			pos->key ^= zobrist_pcsq[piece][rook_castles_to[moveTo(m)]];
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

		if (!getDelimiter(&p) || (pos->side_to_move = getSideToMove(&p)) == -1) {
			return 4;
		}
		p++;

		if (!getDelimiter(&p) || setupCastling(&p) == -1) {
			return 5;
		}
		Square sq;

		if (!getDelimiter(&p) || (int)(sq = getEPsquare(&p)) == -1) {
			return 6;
		}
		pos->en_passant_square = sq < 64 ? bbSquare(sq) : 0;
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

	char* getFen()
	{
		static char piece_letter[] = { "PNBRQK  pnbrqk" };
		static char fen[128];
		char* p = fen;
		char buf[12];

		memset(p, 0, 128);

		for (char r = 7; r >= 0; r--) {
			int empty = 0;

			for (char f = 0; f <= 7; f++) {
				Square sq = r * 8 + f;
				Piece pc = board.getPiece(sq);

				if (pc != NoPiece) {
					if (empty) {
						*(p++) = empty + '0';
						empty = 0;
					}
					*(p++) = piece_letter[pc];
				}
				else {
					empty++;
				}
			}

			if (empty) {
				*(p++) = empty + '0';
			}

			if (r > 0) {
				*(p++) = '/';
			}
		}
		memcpy(p, pos->side_to_move == 0 ? " w " : " b ", 3);
		p += 3;

		if (pos->castle_rights == 0) {
			memcpy(p, "- ", 2);
			p += 2;
		}
		else {
			if (pos->castle_rights & 1) {
				*(p++) = 'K';
			}
			if (pos->castle_rights & 2) {
				*(p++) = 'Q';
			}
			if (pos->castle_rights & 4) {
				*(p++) = 'k';
			}
			if (pos->castle_rights & 8) {
				*(p++) = 'q';
			}
			*(p++) = ' ';
		}

		if (pos->en_passant_square) {
			Square ep = lsb(pos->en_passant_square);
			memcpy(p, squareToString(ep, buf), 2);
			p += 2;
			*(p++) = ' ';
		}
		else {
			memcpy(p, "- ", 2);
			p += 2;
		}
		memset(buf, 0, 12);
		memcpy(p, itoa(pos->reversible_half_move_count, buf, 10), 12);
		p[strlen(p)] = ' ';
		memset(buf, 0, 12);
		memcpy(p + strlen(p), itoa(static_cast<int>((pos - position_list)/2 + 1), buf, 10), 12);
		return fen;
	}

	char getEPsquare(const char** p) {
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
	/*
		fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
		fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1
		fen r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1
		fen r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1
	*/
	int setupCastling(const char** p) {
		for (int sq = 0; sq < 64; sq++) {
			castle_rights_mask[sq] = 15;
		}

		if (**p == '-') {
			(*p)++;
			return 0;
		}
		int i = 0;

		while (**p != 0 && **p != ' ') {
			if (i == 4) {
				return -1;
			}
			char c = **p;

			if (c >= 'A' && c <= 'H') {
				chess960 = true;
				xfen = false;

				int rook_file = c - 'A';

				if (rook_file > file(board.king_square[0])) {
					addShortCastleRights(0, rook_file);
				}
				else {
					addLongCastleRights(0, rook_file);
				}
			}
			else if (c >= 'a' && c <= 'h') {
				chess960 = true;
				xfen = false;

				int rook_file = c - 'a';

				if (rook_file > file(board.king_square[1])) {
					addShortCastleRights(1, rook_file);
				}
				else {
					addLongCastleRights(1, rook_file);
				}
			}
			else {
				if (c == 'K') {
					addShortCastleRights(0, -1);
				}
				else if (c == 'Q') {
					addLongCastleRights(0, -1);
				}
				else if (c == 'k') {
					addShortCastleRights(1, -1);
				}
				else if (c == 'q') {
					addLongCastleRights(1, -1);
				}
				else if (c != '-') {
					return -1;
				}
			}
			(*p)++;
		}
		return 0;
	}

	void addShortCastleRights(int side, int rook_file) {
		if (rook_file == -1) {
			for (int i = 7; i >= 0; i--) {
				if (board.getPieceType(i + side*56) == Rook) {
					rook_file = i; // right outermost rook for side
					break;
				}
			}
			xfen = true;
		}
		pos->castle_rights |= side == 0 ? 1 : 4;
		castle_rights_mask[flip[side ^ 1][rook_file]] -= oo_allowed_mask[side];
		castle_rights_mask[flip[side ^ 1][file(board.king_square[side])]] -= oo_allowed_mask[side];
		rook_castles_from[flip[side ^ 1][g1]] = flip[side ^ 1][rook_file];
		oo_king_from[side] = board.king_square[side];

		if (file(board.king_square[side]) != 4 || rook_file != 7) {
			chess960 = true;
		}
	}

	void addLongCastleRights(int side, int rook_file) {
		if (rook_file == -1) {
			for (int i = 0; i <= 7; i++) {
				if (board.getPieceType(i + side*56) == Rook) {
					rook_file = i; // left outermost rook for side
					break;
				}
			}
			xfen = true;
		}
		pos->castle_rights |= side == 0 ? 2 : 8;
		castle_rights_mask[flip[side ^ 1][rook_file]] -= ooo_allowed_mask[side];
		castle_rights_mask[flip[side ^ 1][file(board.king_square[side])]] -= ooo_allowed_mask[side];
		rook_castles_from[flip[side ^ 1][c1]] = flip[side ^ 1][rook_file];
		ooo_king_from[side] = board.king_square[side];

		if (file(board.king_square[side]) != 4 || rook_file != 0) {
			chess960 = true;
		}
	}

	char getSideToMove(const char** p) {
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

	bool getDelimiter(const char** p) {
		if (**p != ' ') {
			return false;
		}
		(*p)++;
		return true;
	}

	void copy(Game* other) {
		board = other->board;
		chess960 = other->chess960;
		xfen = other->xfen;

		pos += (other->pos - other->position_list);

		for (int i = 0; i < 2000; i++) {
			position_list[i] = other->position_list[i];
			position_list[i].board = &board;
		}
	}

	const char* moveToString(const Move m, char* buf) {
		char tmp1[12];
		char tmp2[12];
		char tmp3[12];

		if (isCastleMove(m) && chess960) {
			if (xfen && moveTo(m) == ooo_king_to[moveSide(m)]) {
				strcpy(buf, "O-O-O");
			}
			else if (xfen) {
				strcpy(buf, "O-O");
			}
			else { // shredder fen
				sprintf(buf, "%s%s", squareToString(moveFrom(m), tmp1),
					squareToString(rook_castles_from[moveTo(m)], tmp2));
			}
		}
		else {
			sprintf(buf, "%s%s", squareToString(moveFrom(m), tmp1), squareToString(moveTo(m), tmp2));

			if (isPromotion(m)) {
				sprintf(&buf[strlen(buf)], "%s", pieceToString(movePromoted(m) & 7, tmp3));
			}
		}
		return buf;
	}

	void print_moves() {
		int i = 0;
		char buf[12];
		while (const MoveData* m = pos->nextMove()) {
			printf("%d. ", i++ + 1);
			printf("%s", moveToString(m->move, buf));
			printf("   %d\n", m->score);
		}
	}

public:
	Position* position_list;
	Position* pos;
	Board board;
	bool chess960;
	bool xfen;

	static const char START_POSITION[];
};

const char Game::START_POSITION[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
