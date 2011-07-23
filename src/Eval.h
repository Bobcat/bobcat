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
	Eval(Game* game, PawnStructureTable* pawnt) {
		initialise(game, pawnt);
	}

	void newGame() {
	}

	int evaluate() {
		initialiseEvaluate();

		// Pass 1.
		evalPawnsBothSides();
		for (Side side = 0; side < 2; side++) {
			evalMaterialOneSide(side);
			evalKingOneSide(side);
			evalRooksOneSide(side);
			evalKnightsOneSide(side);
			evalBishopsOneSide(side);
			evalQueensOneSide(side);
			if (pawn_attacks[side] & king_area[side ^ 1]) {
				attack_count[side]++;
				attack_points[side] += 2;
			}
		}
		// Pass 2.
		for (Side side = 0; side < 2; side++) {
			evalPassedPawnsOneSide(side);
			evalKingAttackOneSide(side);
		}

		double stage = (pos->material.value()-pos->material.pawnValue())/
			(double)pos->material.max_value_no_pawns;

		int pos_eval = (int)(((poseval_mg[0]-poseval_mg[1])*stage)
						+ ((poseval_eg[0]-poseval_eg[1])*(1-stage))
						+ (poseval[0]-poseval[1]));

		int mat_eval = mateval[0] - mateval[1];
		//stage = (pos->material.value())/(double)pos->material.max_value;
		//mat_eval = (int)(mat_eval*(stage + 1.2*(1-stage))); 
		
		int eval = pos_eval + mat_eval;

		return pos->material.evaluate(pos->flags, pos->side_to_move == 1 ? -eval : eval, 
			pos->side_to_move, board, all_attacks);
	}

	__forceinline const BB& attacks(Side side) {
		return all_attacks[side];
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

	__forceinline void evalPawnsOneSide(const Side side) {
		for (BB bb = pawns(side); bb; ) {
			Square sq = lsb(bb);

			pawn_eval_mg[side] += pawn_pcsq_mg[flip[side][sq]];
			pawn_eval_eg[side] += pawn_pcsq_eg[flip[side][sq]];

			if (board->isPawnPassed(sq, side)) {
				passed_pawn_files[side] |= 1 << file(sq);
			}
			bool open_file = !board->isPieceOnFile(Pawn, sq, side ^ 1);
			if (board->isPawnIsolated(sq, side)) {
				pawn_eval_mg[side] += open_file ? -40 : -20; 
				pawn_eval_eg[side] += -20;
			}
			else if (board->isPawnBehind(sq, side)) {
				pawn_eval_mg[side] += open_file ? -20 : -8;
				pawn_eval_eg[side] += -8;
			}
			resetLSB(bb);
			if (bbFile(sq) & bb) {
				pawn_eval_mg[side] += -15; 
				pawn_eval_eg[side] += -15;
			}
		}
	}

	__forceinline void evalKnightsOneSide(const Side side) {
		for (BB knights = board->knights(side); knights; resetLSB(knights)) {
			Square sq = lsb(knights);

			Square flipsq = flip[side][sq]; 
			poseval_mg[side] += knight_pcsq_mg[flipsq];
			poseval_eg[side] += knight_pcsq_eg[flipsq];

			const BB& attacks = knight_attacks[sq];
			int x = popCount(attacks & not_occupied & ~pawn_attacks[side ^ 1]);
			poseval[side] += -40 + 5*x;
			all_attacks[side] |= attacks;

			bool outpost = (passed_pawn_front_span[side][sq] & (pawns(side ^ 1) & ~pawn_front_span[side][sq])) == 0;
			if (outpost && (pawn_attacks[side] & bbSquare(sq))) {
				int d = 7 - distance[sq][kingSq(side ^ 1)];
				poseval[side] += 5*d;
				poseval_eg[side] += 2*d;
			}
			int cnt = popCount(attacks & king_area[side ^ 1])*8;
			attack_points[side] += cnt;
			attack_count[side] += cnt ? 1 : 0;
		}
	}

	__forceinline void evalBishopsOneSide(const Side side) {
		for (BB bishops = board->bishops(side); bishops; resetLSB(bishops)) {
			Square sq = lsb(bishops);
			const BB& bbsq = bbSquare(sq);

			Square flipsq = flip[side][sq]; 
			poseval_mg[side] += bishop_pcsq_mg[flipsq];
			poseval_eg[side] += bishop_pcsq_eg[flipsq];

			const BB attacks = Bmagic(sq, occupied);
			int x = popCount(attacks & not_occupied);
			poseval[side] += -50 + 6*x;
			all_attacks[side] |= attacks;
			
			if (bishop_trapped_a7h7[side] & bbsq) {
				int x = file(sq)/7;
				if ((pawns(side ^ 1) & pawns_trap_bishop_a7h7[x][side]) == pawns_trap_bishop_a7h7[x][side]) {
					poseval[side] -= 110;
				}
			}
			bool outpost = (passed_pawn_front_span[side][sq] & (pawns(side ^ 1) & ~pawn_front_span[side][sq])) == 0;
			if (outpost && (pawn_attacks[side] & bbsq)) {
				int d = 7 - distance[sq][kingSq(side ^ 1)];
				poseval[side] += 5*d;
				poseval_eg[side] += 2*d;
			}
			const BB attacks2 = Bmagic(sq, occupied & ~board->queens(side) & ~board->rooks(side ^ 1) & 
				~board->queens(side ^ 1));

			int cnt = popCount(attacks2 & king_area[side ^ 1])*6;
			attack_points[side] += cnt;
			attack_count[side] += cnt ? 1 : 0;
		}
	}

	__forceinline void evalRooksOneSide(const Side side) {
		for (BB rooks = board->rooks(side); rooks; resetLSB(rooks)) {
			Square sq = lsb(rooks);
			const BB& bbsq = bbSquare(sq); 

			Square flipsq = flip[side][sq]; 
			poseval_mg[side] += rook_pcsq_mg[flipsq];
			//poseval_eg[side] += rook_pcsq_eg[flipsq];

			if (bbsq & open_files) { 
				poseval[side] += 20;
			}
			else if (bbsq & half_open_files[side]) { 
				poseval[side] += 10;
			}
			if ((bbsq & rank_7[side]) && (rank_7_and_8[side] & (pawns(side ^ 1) | board->king(side ^ 1)))) {
				poseval[side] += 20;
			}
			const BB attacks = Rmagic(sq, occupied);
			int x = popCount(attacks & not_occupied);
			poseval_mg[side] += -20 + 2*x;
			poseval_eg[side] += -50 + 5*x;
			all_attacks[side] |= attacks;

			const BB attacks2 = Rmagic(sq, occupied & ~board->rooks(side) & ~board->queens(side) & 
				~board->queens(side ^ 1));

			int cnt = popCount(attacks2 & king_area[side ^ 1])*12;
			attack_points[side] += cnt;
			attack_count[side] += cnt ? 1 : 0;
		}
	}

	__forceinline void evalQueensOneSide(const Side side) {
		for (BB queens = board->queens(side); queens; resetLSB(queens)) {
			Square sq = lsb(queens);
			const BB& bbsq = bbSquare(sq);

			Square flipsq = flip[side][sq]; 
			poseval_mg[side] += queen_pcsq_mg[flipsq];
			poseval_eg[side] += queen_pcsq_eg[flipsq];

			if ((bbsq & rank_7[side]) && (rank_7_and_8[side] & (pawns(side ^ 1) | board->king(side ^ 1)))) {
				poseval[side] += 20;
			}
			const BB attacks = Qmagic(sq, occupied);
			all_attacks[side] |= attacks;

			int d = 7 - distance[sq][kingSq(side ^ 1)];
			poseval[side] += 5*d;
			poseval_eg[side] += 2*d;

			const BB attacks2 = Bmagic(sq, occupied & ~board->bishops(side) & ~board->queens(side)) | 
				Rmagic(sq, occupied & ~board->rooks(side) & ~board->queens(side));

			int cnt = popCount(attacks2 & king_area[side ^ 1])*24;
			attack_points[side] += cnt;
			attack_count[side] += cnt ? 1 : 0;
		}
	}

	__forceinline void evalMaterialOneSide(const Side side) {
		mateval[side] += pos->material.material_value[side];
		if (pos->material.count(side, Bishop) == 2) {
			poseval[side] += max(30, 64 - (pos->material.pawnCount()*4));
		}
	}

	__forceinline void evalKingOneSide(const Side side) {
		Square sq = lsb(board->king(side));
		const BB& bbsq = bbSquare(sq);

		poseval_mg[side] += king_pcsq_mg[flip[side][sq]];
		poseval_eg[side] += king_pcsq_eg[flip[side][sq]];

		int pawn_shield = -45 + 15*popCount((pawnPush[side](bbsq) | pawnWestAttacks[side](bbsq) | 
			pawnEastAttacks[side](bbsq)) & pawns(side));

		poseval_mg[side] += pawn_shield; 

		if (board->queens(side ^ 1) || popCount(board->rooks(side ^ 1)) > 1) {
			BB eastwest = bbsq | westOne(bbsq) | eastOne(bbsq);
			poseval_mg[side] += -25*popCount(open_files & eastwest);
			poseval_mg[side] += -20*popCount(half_open_files[side] & eastwest);
		}

		if (((side == 0) && 
				(((sq == f1 || sq == g1) && (bbSquare(h1) & board->rooks(0))) || 
				((sq == c1 || sq == b1) && (bbSquare(a1) & board->rooks(0))))) ||
			((side == 1) && 
				(((sq == f8 || sq == g8) && (bbSquare(h8) & board->rooks(1))) || 
				((sq == c8 || sq == b8) && (bbSquare(a8) & board->rooks(1))))))
		{
			poseval_mg[side] += -180;
		}
		all_attacks[side] |= king_attacks[kingSq(side)];
	}

	__forceinline void evalPassedPawnsOneSide(const Side side) {
		for (BB files = pawnp ? pawnp->passed_pawn_files[side] : 0; files; resetLSB(files)) {
			for (BB bb = bbFile(lsb(files)) & pawns(side); bb; resetLSB(bb)) {
				int sq = lsb(bb);
				const BB& front_span = pawn_front_span[side][sq];
				int r = side == 0 ? rank(sq) : 7 - rank(sq);
				int rr = r*r;
				
				int score_mg = rr*4; 
				int score_eg = rr*3; 

				score_eg += rr*(front_span & board->occupied_by_side[side] ? 0 : 1);
				score_eg += rr*(front_span & board->occupied_by_side[side ^ 1] ? 0 : 1);
				score_eg += rr*(front_span & all_attacks[side ^ 1] ? 0 : 1);

				score_eg += r*(distance[sq][kingSq(side ^ 1)]*2-distance[sq][kingSq(side)]*2);

				poseval_mg[side] += score_mg;
				poseval_eg[side] += score_eg;
			}
		}
	}

	__forceinline void evalKingAttackOneSide(const Side side) {
		if (attack_count[side] > 1) {
			poseval_mg[side] += attack_points[side]*(attack_count[side] - 1);
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

		poseval_mg[0] = poseval_eg[0] = poseval[0] = mateval[0] = 0;
		poseval_mg[1] = poseval_eg[1] = poseval[1] = mateval[1] = 0;

		attack_points[0] = attack_points[1] = attack_count[0] = attack_count[1] = 0;

		king_area[0] = king_attacks[kingSq(0)] | board->king(0);
		king_area[1] = king_attacks[kingSq(1)] | board->king(1);

		occupied = board->occupied;
		not_occupied = ~occupied;

		open_files = ~(northFill(southFill(pawns(0))) | northFill(southFill(pawns(1))));
		half_open_files[0] = ~northFill(southFill(pawns(0))) & ~open_files;
		half_open_files[1] = ~northFill(southFill(pawns(1))) & ~open_files;
		all_attacks[0] = pawn_attacks[0] = pawnEastAttacks[0](pawns(0)) | pawnWestAttacks[0](pawns(0));
		all_attacks[1] = pawn_attacks[1] = pawnEastAttacks[1](pawns(1)) | pawnWestAttacks[1](pawns(1));
	}

	void initialise(Game* game, PawnStructureTable* pawnt) {
		this->game = game;
		board = game->pos->board;
		this->pawnt = pawnt;
		pawns_array[0] = &board->pawns(0);
		pawns_array[1] = &board->pawns(1);
		king_square[0] = &board->king_square[0];
		king_square[1] = &board->king_square[1];
	}

	Board* board;
	Position* pos;
	Game* game;
	PawnStructureTable* pawnt;
	PawnEntry* pawnp;

	int poseval_mg[2];
	int poseval_eg[2];
	int poseval[2];
	int mateval[2];
	int pawn_eval_mg[2];
	int pawn_eval_eg[2];
	int passed_pawn_files[2];
	int attack_points[2];
	int attack_count[2];

	BB pawn_attacks[2];
	BB all_attacks[2];
	BB king_area[2];
	BB occupied;
	BB not_occupied;
	BB open_files;
	BB half_open_files[2];

	const BB* pawns_array[2];
	const Square* king_square[2];

	static int pawn_pcsq_mg[64];
	static int pawn_pcsq_eg[64];
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
	
	static int bishop_mobility_mg[14];
	static int rook_mobility_mg[15];
	static int knight_mobility_mg[9];

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

int Eval::bishop_mobility_mg[14] = { 
	-15, -6, 0, 6, 12, 16, 19, 21, 22, 22, 22, 23, 23, 23
};

int Eval::rook_mobility_mg[15] = { 
	-15, -7, 0, 6, 12, 16, 19, 21, 22, 22, 22, 23, 23, 23, 24 
};

int Eval::knight_mobility_mg[9] = { 
	-15, -8, -4, 0, 3, 6, 9, 12, 15
};

int Eval::pawn_pcsq_mg[64] = {
   0,   0,   0,   0,   0,   0,   0,   0,
  -5,   7,  13,  17,  17,  13,   7,  -5,
  -6,   6,  12,  16,  16,  12,   6,  -6,
  -7,   5,  11,  15,  15,  11,   5,  -7,
 -10,   2,   8,  12,  12,   8,   2, -10,
 -11,   1,   7, -10, -10,   7,   1, -11,
 -12,   0,   6, -40, -40,   6,   0, -12,
   0,   0,   0,   0,   0,   0,   0,   0
};

int Eval::pawn_pcsq_eg[64] = {
   0,   0,   0,   0,   0,   0,   0,   0,
   0,  -2,  -5,  -7,  -7,  -5,  -2,   0,
  -3,  -5,  -8, -10, -10,  -8,  -5,  -3,
  -6,  -8, -11, -13, -13, -11,  -8,  -6,
  -7,  -9, -12, -14, -14, -12,  -9,  -7,
  -8, -10, -13, -15, -15, -13, -10,  -8,
  -9, -11, -14, -16, -16, -14, -11,  -9,
   0,   0,   0,   0,   0,   0,   0,   0
};

int Eval::knight_pcsq_mg[64] = {
 -95, -20, -10,  -5,  -5, -10, -20, -95,
 -16,  -1,  10,  14,  14,  10,  -1, -16,
  -7,   8,  18,  23,  23,  18,   8,  -7,
  -5,  10,  20,  25,  25,  20,  10,  -5,
 -10,   5,  15,  20,  20,  15,   5, -10,
 -20,  -5,   5,  10,  10,   5,  -5, -20,
 -35, -20, -10,  -5,  -5, -10, -20, -35,
 -55, -40, -30, -25, -25, -30, -40, -55
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
  -9,   0,   6,  15,  15,   6,   0,  -9,
  -9,   0,   6,  15,  15,   6,   0,  -9,
  -6,   0,   7,   6,   6,   7,   0,  -6,
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

