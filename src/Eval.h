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

class Eval {
public:
	Eval(Game* game, PawnStructureTable* pawnt, SEE* see) {
		initialise(game, pawnt, see);
	}

	virtual ~Eval() {
	}

	virtual void newGame() {
	}

	int evaluate(int alpha, int beta) {
		initialiseEvaluate();

		evalMaterialOneSide(0);
		evalMaterialOneSide(1);

		int mat_eval = mateval[0] - mateval[1];

		if (pos->use_lazy) {
			int lazy_margin = 300;
			int lazy_eval = pos->side_to_move == 0 ? mat_eval : -mat_eval;

			if (lazy_eval - lazy_margin > beta || lazy_eval + lazy_margin < alpha) {
				return pos->material.evaluate(pos->flags, lazy_eval, pos->side_to_move, board);
			}
		}

		// Pass 1.
		evalPawnsBothSides();
		evalKnightsOneSide(0);
		evalKnightsOneSide(1);
		evalBishopsOneSide(0);
		evalBishopsOneSide(1);
		evalRooksOneSide(0);
		evalRooksOneSide(1);
		evalQueensOneSide(0);
		evalQueensOneSide(1);
		evalKingOneSide(0);
		evalKingOneSide(1);

		for (Side side = 0; side < 2; side++) {
			if (pawn_attacks[side] & king_area[side^1]) {
				attack_count[side]++;
				attack_counter[side] += 2;
			}
		}
		// Pass 2.
		for (Side side = 0; side < 2; side++) {
			evalPassedPawnsOneSide(side);
			evalKingAttackOneSide(side);
		}
		double stage = (pos->material.value()-pos->material.pawnValue())/
			(double)pos->material.max_value_without_pawns;

		poseval[pos->side_to_move] += 10;

		int pos_eval_mg = (int)((poseval_mg[0]-poseval_mg[1])*stage);
		int pos_eval_eg = (int)((poseval_eg[0]-poseval_eg[1])*(1-stage));
		int pos_eval = pos_eval_mg + pos_eval_eg + (poseval[0] - poseval[1]);

		pos->use_lazy = abs(pos_eval_mg) < 150 && abs(pos_eval_eg) < 150 && abs(pos_eval) < 150;

		int eval = pos_eval + mat_eval;

		return pos->material.evaluate(pos->flags, pos->side_to_move == 1 ? -eval : eval,
			pos->side_to_move, board);
	}

protected:
	__forceinline void evalPawnsBothSides() {
		pawnp = 0;

		if (pos->material.pawnCount()) {
			pawnp = pawnt->find(pos->pawn_structure_key);

			if (!pawnp) {
				pawn_eval_mg[0] = pawn_eval_mg[1] = 0;
				pawn_eval_eg[0] = pawn_eval_eg[1] = 0;

				passed_pawn_files[0] = passed_pawn_files[1] = 0;

				evalPawnsOneSide(0);
				evalPawnsOneSide(1);

				pawnp = pawnt->insert(pos->pawn_structure_key, (int)(pawn_eval_mg[0] - pawn_eval_mg[1]),
					(int)(pawn_eval_eg[0] - pawn_eval_eg[1]), passed_pawn_files);
			}
			poseval_mg[0] += pawnp->eval_mg;
			poseval_eg[0] += pawnp->eval_eg;
		}
	}

	__forceinline void evalPawnsOneSide(const Side us) {
		const Side them = us ^ 1;

		for (BB bb = pawns(us); bb; ) {
			Square sq = lsb(bb);

			int score_mg = 0;
			int score_eg = 0;

			if (board->isPawnPassed(sq, us)) {
				passed_pawn_files[us] |= 1 << file(sq);
			}
			bool open_file = !board->isPieceOnFile(Pawn, sq, them);

			if (board->isPawnIsolated(sq, us)) {
				score_mg += open_file ? -40 : -20;
				score_eg += -20;
			}
			else if ((bbSquare(sq) & pawn_attacks[us]) == 0) {
				score_mg += open_file ? -20 : -8;
				score_eg += -8;
			}
			resetLSB(bb);

			if (bbFile(sq) & bb) {
				score_mg += -15;
				score_eg += -15;
			}
			int r = us == 0 ? rank(sq) - 1 : 6 - rank(sq);

			score_mg += 4*r;
			score_eg += 2*r;

			pawn_eval_mg[us] += score_mg;
			pawn_eval_eg[us] += score_eg;
		}
	}

	__forceinline void evalKnightsOneSide(const Side us) {
		const Side them = us ^ 1;
		for (BB knights = board->knights(us); knights; resetLSB(knights)) {
			Square sq = lsb(knights);
			Square flipsq = flip[us][sq];

			int score_mg = knight_pcsq_mg[flipsq];
			int score_eg = knight_pcsq_eg[flipsq];

			const BB& attacks = knight_attacks[sq];
			int x = popCount(attacks & ~board->occupied_by_side[us] & ~pawn_attacks[them]);

			int score = 5*x;

			all_attacks[us] |= attacks;
			_knight_attacks[us] |= attacks;

			bool outpost = (passed_pawn_front_span[us][sq] & (pawns(them) & ~pawn_front_span[us][sq])) == 0;

			if (outpost && (pawn_attacks[us] & bbSquare(sq))) {
				score += std::max(0, knight_pcsq_eg[flipsq]);
			}

			if (attacks & king_area[them]) {
				attack_counter[us] += popCount(attacks & king_area[them])*8;
				attack_count[us]++;
			}

			if (bbSquare(sq) & pawn_attacks[them]) {
				score += -14;
			}
			poseval[us] += score;
			poseval_mg[us] += score_mg;
			poseval_eg[us] += score_eg;
		}
	}

	__forceinline void evalBishopsOneSide(const Side us) {
		const Side them = us ^ 1;
		for (BB bishops = board->bishops(us); bishops; resetLSB(bishops)) {
			Square sq = lsb(bishops);
			const BB& bbsq = bbSquare(sq);
			Square flipsq = flip[us][sq];

			int score_mg = bishop_pcsq_mg[flipsq];
			int score_eg = bishop_pcsq_eg[flipsq];

			const BB attacks = Bmagic(sq, occupied);
			int x = popCount(attacks & ~(board->occupied_by_side[us]));

			int score = 6*x;

			all_attacks[us] |= attacks;
			bishop_attacks[us] |= attacks;

			if (bishop_trapped_a7h7[us] & bbsq) {
				int x = file(sq)/7;
				if ((pawns(them) & pawns_trap_bishop_a7h7[x][us]) == pawns_trap_bishop_a7h7[x][us]) {
					score -= 110;
				}
			}

			if (attacks & king_area[them]) {
				attack_counter[us] += popCount(attacks & king_area[them])*6;
				attack_count[us]++;
			}

			if (bbSquare(sq) & pawn_attacks[them]) {
				score += -14;
			}
			poseval[us] += score;
			poseval_mg[us] += score_mg;
			poseval_eg[us] += score_eg;
		}
	}

	__forceinline void evalRooksOneSide(const Side us) {
		const Side them = us ^ 1;
		for (BB rooks = board->rooks(us); rooks; resetLSB(rooks)) {
			Square sq = lsb(rooks);
			const BB& bbsq = bbSquare(sq);
			Square flipsq = flip[us][sq];

			int score_mg = rook_pcsq_mg[flipsq];
			int score_eg = rook_pcsq_eg[flipsq];
			int score = 0;

			if (bbsq & open_files) {
				score += 20;

				if (~bbsq & bbFile(sq) & board->rooks(us)) {
					score += 20;
				}
			}
			else if (bbsq & half_open_files[us]) {
				score += 10;

				if (~bbsq & bbFile(sq) & board->rooks(us)) {
					score += 10;
				}
			}

			if ((bbsq & rank_7[us]) && (rank_7_and_8[us] & (pawns(them) | board->king(them)))) {
				score += 20;
			}
			const BB attacks = Rmagic(sq, occupied);
			int x = popCount(attacks & ~board->occupied_by_side[us]);

			score_mg += 2*x;
			score_eg += 5*x;

			all_attacks[us] |= attacks;
			rook_attacks[us] |= attacks;

			if (attacks & king_area[them]) {
				attack_counter[us] += popCount(attacks & king_area[them])*12;
				attack_count[us]++;
			}

			if (bbSquare(sq) & (pawn_attacks[them] | _knight_attacks[them] | bishop_attacks[them])) {
				score += -18;
			}
			poseval[us] += score;
			poseval_mg[us] += score_mg;
			poseval_eg[us] += score_eg;
		}
	}

	__forceinline void evalQueensOneSide(const Side us) {
		const Side them = us ^ 1;
		for (BB queens = board->queens(us); queens; resetLSB(queens)) {
			Square sq = lsb(queens);
			const BB& bbsq = bbSquare(sq);
			Square flipsq = flip[us][sq];

			int score_mg = 0;//queen_pcsq_mg[flipsq];
			int score_eg = queen_pcsq_mg[flipsq];
			int d = 7 - distance[sq][kingSq(them)];
			int score = 5*d;

			score_eg += 2*d;

			if ((bbsq & rank_7[us]) && (rank_7_and_8[us] & (pawns(them) | board->king(them)))) {
				score += 20;
			}
			const BB attacks = Qmagic(sq, occupied);

			all_attacks[us] |= attacks;
			queen_attacks[us] |= attacks;

			if (attacks & king_area[them]) {
				attack_counter[us] += popCount(attacks & king_area[them])*24;
				attack_count[us]++;
			}

			if (bbSquare(sq) & (pawn_attacks[them] | _knight_attacks[them] | bishop_attacks[them] | rook_attacks[them])) {
				score += -20;
			}
			poseval[us] += score;
			poseval_mg[us] += score_mg;
			poseval_eg[us] += score_eg;
		}
	}

	__forceinline void evalMaterialOneSide(const Side side) {
		mateval[side] = pos->material.material_value[side];

		if (pos->material.count(side, Bishop) == 2) {
			mateval[side] += 30;

			if (pos->material.pawnCount() <= 12) {
				mateval[side] += 20;
			}
		}
	}

	__forceinline void evalKingOneSide(const Side us) {
		const Side them = us ^ 1;
		Square sq = lsb(board->king(us));
		const BB& bbsq = bbSquare(sq);

		int score_mg = king_pcsq_mg[flip[us][sq]];
		int score_eg = king_pcsq_eg[flip[us][sq]];

		score_mg += -45 + 15*popCount((pawnPush[us](bbsq) | pawnWestAttacks[us](bbsq) |
			pawnEastAttacks[us](bbsq)) & pawns(us));

		if (board->queens(them) || popCount(board->rooks(them)) > 1) {
			BB eastwest = bbsq | westOne(bbsq) | eastOne(bbsq);
			int x = -10*popCount(open_files & eastwest);
			int y = -5*popCount(half_open_files[us] & eastwest);

			score_mg += x;
			score_mg += y;

			if (board->queens(them) && popCount(board->rooks(them))) {
				score_mg += 2*x;
				score_mg += 2*y;

				if (popCount(board->rooks(them) > 1)) {
					score_mg += 3*x;
					score_mg += 3*y;
				}
			}
		}

		if (((us == 0) &&
				(((sq == f1 || sq == g1) && (bbSquare(h1) & board->rooks(0))) ||
				((sq == c1 || sq == b1) && (bbSquare(a1) & board->rooks(0))))) ||
			((us == 1) &&
				(((sq == f8 || sq == g8) && (bbSquare(h8) & board->rooks(1))) ||
				((sq == c8 || sq == b8) && (bbSquare(a8) & board->rooks(1))))))
		{
			score_mg += -80;
		}

		all_attacks[us] |= king_attacks[kingSq(us)];
		poseval_mg[us] += score_mg;
		poseval_eg[us] += score_eg;
	}

	__forceinline void evalPassedPawnsOneSide(const Side us) {
		const Side them = us ^ 1;
		for (BB files = pawnp ? pawnp->passed_pawn_files[us] : 0; files; resetLSB(files)) {
			for (BB bb = bbFile(lsb(files)) & pawns(us); bb; resetLSB(bb)) {
				int sq = lsb(bb);
				const BB& front_span = pawn_front_span[us][sq];
				int r = us == 0 ? rank(sq) : 7 - rank(sq);
				int rr = r*r;

				int score_mg = rr*4;
				int score_eg = rr*3;

				score_eg += rr*(front_span & board->occupied_by_side[us] ? 0 : 1);
				score_eg += rr*(front_span & board->occupied_by_side[them] ? 0 : 1);
				score_eg += rr*(front_span & all_attacks[them] ? 0 : 1);
				score_eg += (r-1)*r*(distance[sq][kingSq(them)]-distance[sq][kingSq(us)]);

				poseval_mg[us] += score_mg;
				poseval_eg[us] += score_eg;
			}
		}
	}

	__forceinline void evalKingAttackOneSide(const Side side) {
		if (attack_count[side] > 1) {
			poseval_mg[side] += attack_counter[side]*(attack_count[side]-1);
		}
	}

	__forceinline const BB& pawns(Side side) {
		return *pawns_array[side];
	}

	__forceinline Square kingSq(Side side) {
		return *king_square[side];
	}

	__forceinline void initialiseEvaluate() {
		pos = game->pos;
		pos->flags = 0;

		poseval_mg[0] = poseval_eg[0] = poseval[0] = 0;
		poseval_mg[1] = poseval_eg[1] = poseval[1] = 0;

		attack_counter[0] = attack_counter[1] = 0;
		attack_count[0] = attack_count[1] = 0;

		king_area[0] = king_attacks[kingSq(0)] | board->king(0);
		king_area[1] = king_attacks[kingSq(1)] | board->king(1);

		occupied = board->occupied;
		not_occupied = ~occupied;

		open_files = ~(northFill(southFill(pawns(0))) | northFill(southFill(pawns(1))));
		half_open_files[0] = ~northFill(southFill(pawns(0))) & ~open_files;
		half_open_files[1] = ~northFill(southFill(pawns(1))) & ~open_files;

		all_attacks[0] = pawn_attacks[0] = pawnEastAttacks[0](pawns(0)) | pawnWestAttacks[0](pawns(0));
		all_attacks[1] = pawn_attacks[1] = pawnEastAttacks[1](pawns(1)) | pawnWestAttacks[1](pawns(1));

		_knight_attacks[0] = _knight_attacks[1] = 0;
		bishop_attacks[0] = bishop_attacks[1] = 0;
		rook_attacks[0] = rook_attacks[1] = 0;
		queen_attacks[0] = queen_attacks[1] = 0;
	}

	void initialise(Game* game, PawnStructureTable* pawnt, SEE* see) {
		this->game = game;
		board = game->pos->board;
		this->pawnt = pawnt;
		this->see = see;
		pawns_array[0] = &board->pawns(0);
		pawns_array[1] = &board->pawns(1);
		king_square[0] = &board->king_square[0];
		king_square[1] = &board->king_square[1];
	}

	Board* board;
	Position* pos;
	Game* game;
	PawnStructureTable* pawnt;
	SEE* see;
	PawnEntry* pawnp;

	int poseval_mg[2];
	int poseval_eg[2];
	int poseval[2];
	int mateval[2];
	int pawn_eval_mg[2];
	int pawn_eval_eg[2];
	int passed_pawn_files[2];
	int attack_counter[2];
	int attack_count[2];

	BB pawn_attacks[2];
	BB all_attacks[2];
	BB _knight_attacks[2];
	BB bishop_attacks[2];
	BB rook_attacks[2];
	BB queen_attacks[2];
	BB king_area[2];
	BB occupied;
	BB not_occupied;
	BB open_files;
	BB half_open_files[2];

	const BB* pawns_array[2];
	const Square* king_square[2];

	static int knight_pcsq_mg[64];
	static int knight_pcsq_eg[64];
	static int bishop_pcsq_mg[64];
	static int bishop_pcsq_eg[64];
	static int rook_pcsq_mg[64];
	static int rook_pcsq_eg[64];
	static int queen_pcsq_mg[64];
	static int queen_pcsq_eg[64];
	static int king_pcsq_mg[64];
	static int king_pcsq_eg[64];

	static BB bishop_trapped_a7h7[2];
	static BB pawns_trap_bishop_a7h7[2][2];
};

BB Eval::bishop_trapped_a7h7[2] = {
	(BB)1 << a7 | (BB)1 << h7, (BB)1 << a2 | (BB)1 << h2
};

BB Eval::pawns_trap_bishop_a7h7[2][2] = {
	{ (BB)1 << b6 | (BB)1 << c7, (BB)1 << b3 | (BB)1 << c2 },
	{ (BB)1 << g6 | (BB)1 << f7, (BB)1 << g3 | (BB)1 << f2 }
};

int Eval::knight_pcsq_mg[64] = {
 -48, -20, -10,  -5,  -5, -10, -20, -48,
 -16,  -1,  10,  14,  14,  10,  -1, -16,
  -8,   8,  18,  23,  23,  18,   8,  -8,
  -4,  10,  20,  25,  25,  20,  10,  -4,
  -8,   5,  15,  20,  20,  15,   5,  -8,
 -16,  -5,   5,  10,  10,   5,  -5, -16,
 -48, -20, -10,  -5,  -5, -10, -20, -48,
 -64, -40, -28, -22, -22, -28, -40, -64
};

int Eval::knight_pcsq_eg[64] = {
 -12,  -8,  -4,  -2,  -2,  -4,  -8, -12,
  -5,  -1,   3,   5,   5,   3,  -1,  -5,
   1,   5,   8,  10,  10,   8,   5,   1,
   0,   4,   6,  10,  10,   6,   4,   0,
  -2,   2,   4,   8,   8,   4,   2,  -2,
  -6,  -2,   2,   4,   4,   2,  -2,  -6,
 -12,  -8,  -4,  -2,  -2,  -4,  -8, -12,
 -20, -16, -12, -10, -10, -12, -16, -20
};

int Eval::bishop_pcsq_mg[64] = {
   3,   0,  -5,  -9,  -9,  -5,   0,   3,
  -2,   4,   0,   0,   0,   0,   4,  -2,
  -6,   0,   7,   8,   8,   7,   0,  -6,
  -9,   0,   6,  12,  12,   6,   0,  -9,
  -9,   0,   6,  10,  10,   6,   0,  -9,
  -6,   0,   0,   0,   0,   0,   0,  -6,
  -2,   4,   0,   0,   0,   0,   4,  -2,
  -6,  -5,  -8, -11, -11,  -8,  -5,  -6
};

int Eval::bishop_pcsq_eg[64] = {
   0,  -1,  -3,  -3,  -3,  -3,  -1,   0,
  -1,   4,   0,   0,   0,   0,   4,  -1,
  -3,   0,   5,   4,   4,   5,   0,  -3,
  -5,   0,   4,   6,   6,   4,   0,  -5,
  -5,   0,   4,   6,   6,   4,   0,  -5,
  -3,   0,   5,   4,   4,   5,   0,  -3,
  -1,   4,   0,   0,   0,   0,   4,  -1,
   0,  -1,  -3,  -5,  -5,  -3,  -1,   0
};

int Eval::rook_pcsq_mg[64] = {
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0,
   0,   2,   5,   7,  7,   5,   2,   0
};

int Eval::rook_pcsq_eg[64] = {
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0
};

int Eval::queen_pcsq_mg[64] = {
 -12,  -7,  -4,  -2,  -2,  -4,  -7, -12,
  -4,  -1,   3,   4,   4,   3,  -1,  -4,
  -1,   2,   6,   7,   7,   6,   2,  -1,
   2,   4,   7,   9,   9,   7,   4,   2,
   2,   4,   7,   9,   9,   7,   4,   2,
  -1,   2,   6,   7,   7,   6,   2,  -1,
  -4,  -1,   3,   6,   6,   3,  -1,  -4,
 -12,  -7,  -4,  -2,  -2,  -4,  -7, -12
};

int Eval::queen_pcsq_eg[64] = {
 -10,  -6,  -4,  -2,  -2,  -4,  -6, -10,
  -4,  -1,   0,   1,   1,   0,  -1,  -4,
  -1,   1,   4,   3,   3,   4,   1,  -1,
   1,   2,   3,   5,   5,   3,   2,   1,
   1,   2,   3,   5,   5,   3,   2,   1,
  -1,   1,   4,   3,   3,   4,   1,  -1,
  -4,  -1,   0,   1,   1,   0,  -1,  -4,
 -10,  -6,  -4,  -2,  -2,  -4,  -6, -10
};

int Eval::king_pcsq_mg[64] = {
   5,  10, -20, -40, -40, -19,  10,   5,
  14,  19, -10, -29, -29,  -8,  19,  14,
  22,  27,   0, -19, -19,   0,  27,  22,
  29,  34,   8, -14, -14,   7,  34,  29,
  34,  39,  14,  -9,  -9,  12,  39,  34,
  36,  41,  16,  -6,  -6,  15,  41,  36,
  38,  43,  18,  -3,  -3,  17,  43,  38,
  40,  45,  20,   0,   0,  20,  45,  40
};

int Eval::king_pcsq_eg[64] = {
 -42, -25, -10,  -5,  -5, -10, -25, -42,
 -22,  -5,   6,  11,  11,   6,  -5, -22,
 -15,   2,  16,  21,  21,  16,   2, -15,
 -17,   8,  25,  30,  30,  25,   8, -17,
 -23,   2,  20,  25,  25,  20,   2, -23,
 -28,  -3,  11,  16,  16,  11,  -3, -28,
 -35, -10,   1,   6,   6,   1, -10, -35,
 -70, -45, -30, -25, -25, -30, -45, -70
};
