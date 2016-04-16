/*
  This file is part of Bobcat.
  Copyright 2008-2015 Gunnar Harms

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
  Eval(const Game& game, PawnStructureTable* pawnt) : game_(game)
  {
    initialise(pawnt);
  }

  virtual ~Eval()
  {
  }

  int evaluate(int alpha, int beta)
  {
    initialiseEvaluate();

    evalMaterialOneSide(0);
    evalMaterialOneSide(1);

    int mat_eval = poseval[0] - poseval[1];

    int lazy_margin = 500;
    int lazy_eval = pos->side_to_move == 0 ? mat_eval : -mat_eval;

    if (lazy_eval - lazy_margin > beta || lazy_eval + lazy_margin < alpha) {
      return pos->material.evaluate(pos->flags, lazy_eval, pos->side_to_move, &game_.board);
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
    int eval = pos_eval;

    return pos->material.evaluate(pos->flags, pos->side_to_move == 1 ? -eval : eval,
                                  pos->side_to_move, &game_.board);
  }

protected:
  __forceinline void evalPawnsBothSides()
  {
    pawnp = 0;

    if (pos->material.pawnCount()) {
      pawnp = tuning_ ? 0 : pawnt->find(pos->pawn_structure_key);

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

  __forceinline void evalPawnsOneSide(const Side us)
  {
    int score_mg = 0;
    int score_eg = 0;

    for (BB bb = pawns(us); bb; ) {
      Square sq = lsb(bb);

      if (game_.board.isPawnPassed(sq, us)) {
        passed_pawn_files[us] |= 1 << fileOf(sq);
      }
      int open_file = !game_.board.isPieceOnFile(Pawn, sq, us^1) ? 1 :0;

      if (game_.board.isPawnIsolated(sq, us)) {
        score_mg += pawn_isolated_mg[open_file];
        score_eg += pawn_isolated_eg[open_file];
      }
      else if (game_.board.isPawnBehind(sq, us)) {
        score_mg += pawn_behind_mg[open_file];
        score_eg += pawn_behind_eg[open_file];
      }
      resetLSB(bb);

      if (bbFile(sq) & bb) {
        score_mg += pawn_doubled_mg[open_file];
        score_eg += pawn_doubled_eg[open_file];
      }
      score_mg += pawn_pcsq_mg[flip[us][sq]];
      score_eg += pawn_pcsq_eg[flip[us][sq]];
    }
    pawn_eval_mg[us] += score_mg;
    pawn_eval_eg[us] += score_eg;
  }

  __forceinline void evalKnightsOneSide(const Side us)
  {
    const Side them = us ^ 1;
    int score_mg = 0;
    int score_eg = 0;
    int score = 0;

    for (BB knights = game_.board.knights(us); knights; resetLSB(knights)) {
      Square sq = lsb(knights);
      Square flipsq = flip[us][sq];

      score_mg += knight_pcsq_mg[flipsq];
      score_eg += knight_pcsq_eg[flipsq];

      const BB& attacks = knight_attacks[sq];
      int x = popCount(attacks & ~game_.board.occupied_by_side[us]);

      score_mg += knight_mob_mg[x];
      score_eg += knight_mob_eg[x];

      int x1 = popCount(attacks & ~game_.board.occupied_by_side[us] & ~pawn_attacks[them]);
      score_mg += knight_mob2_mg[x1];
      score_eg += knight_mob2_eg[x1];

      all_attacks[us] |= attacks;
      _knight_attacks[us] |= attacks;

      if (attacks & king_area[them]) {
        attack_counter[us] += popCount(attacks & king_area[them])*knight_attack_king;
        attack_count[us]++;
      }

      if (bbSquare(sq) & pawn_attacks[them]) {
        score += knight_in_danger;
      }
    }
    poseval[us] += score;
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalBishopsOneSide(const Side us)
  {
    const Side them = us ^ 1;
    int score_mg = 0;
    int score_eg = 0;
    int score = 0;

    for (BB bishops = game_.board.bishops(us); bishops; resetLSB(bishops)) {
      Square sq = lsb(bishops);
      Square flipsq = flip[us][sq];

      score_mg += bishop_pcsq_mg[flipsq];
      score_eg += bishop_pcsq_eg[flipsq];

      const BB attacks = bishopAttacks(sq, occupied);
      int x = popCount(attacks & ~(game_.board.occupied_by_side[us]));

      score_mg += bishop_mob_mg[x];
      score_eg += bishop_mob_eg[x];

      int x1 = popCount(attacks & ~game_.board.occupied_by_side[us] & ~pawn_attacks[them]);
      score_mg += bishop_mob2_mg[x1];
      score_eg += bishop_mob2_eg[x1];

      all_attacks[us] |= attacks;
      bishop_attacks[us] |= attacks;

      if (attacks & king_area[them]) {
        attack_counter[us] += popCount(attacks & king_area[them])*bishop_attack_king;
        attack_count[us]++;
      }

      if (bbSquare(sq) & pawn_attacks[them]) {
        score += bishop_in_danger;
      }
    }
    poseval[us] += score;
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalRooksOneSide(const Side us)
  {
    const Side them = us ^ 1;
    int score_mg = 0;
    int score_eg = 0;
    int score = 0;

    for (BB rooks = game_.board.rooks(us); rooks; resetLSB(rooks)) {
      Square sq = lsb(rooks);
      Square flipsq = flip[us][sq];

      score_mg += rook_pcsq_mg[flipsq];
      score_eg += rook_pcsq_eg[flipsq];

      const BB& bbsq = bbSquare(sq);

      if (bbsq & open_files) {
        score += rook_open_file;
      }
      const BB attacks = rookAttacks(sq, occupied);
      int x = popCount(attacks & ~game_.board.occupied_by_side[us]);

      score_mg += rook_mob_mg[x];
      score_eg += rook_mob_eg[x];

      all_attacks[us] |= attacks;
      rook_attacks[us] |= attacks;

      if (attacks & king_area[them]) {
        attack_counter[us] += popCount(attacks & king_area[them])*rook_attack_king;
        attack_count[us]++;
      }

      if (bbSquare(sq) & (pawn_attacks[them] | _knight_attacks[them] | bishop_attacks[them])) {
        score += rook_in_danger;
      }
    }
    poseval[us] += score;
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalQueensOneSide(const Side us)
  {
    const Side them = us ^ 1;
    int score_mg = 0;
    int score_eg = 0;
    int score = 0;

    for (BB queens = game_.board.queens(us); queens; resetLSB(queens)) {
      Square sq = lsb(queens);
      Square flipsq = flip[us][sq];

      score_mg += queen_pcsq_mg[flipsq];
      score_eg += queen_pcsq_eg[flipsq];

      const BB attacks = queenAttacks(sq, occupied);
      int x = popCount(attacks & ~game_.board.occupied_by_side[us]);

      score_mg += queen_mob_mg[x];
      score_eg += queen_mob_eg[x];

      all_attacks[us] |= attacks;
      queen_attacks[us] |= attacks;

      if (attacks & king_area[them]) {
        attack_counter[us] += popCount(attacks & king_area[them])*queen_attack_king;
        attack_count[us]++;
      }

      if (bbSquare(sq) & (pawn_attacks[them] | _knight_attacks[them] | bishop_attacks[them] | rook_attacks[them])) {
        score += queen_in_danger;
      }
    }
    poseval[us] += score;
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalMaterialOneSide(const Side us)
  {
    poseval[us] = pos->material.material_value[us];

    if (pos->material.count(us, Bishop) == 2) {
      poseval_mg[us] += bishop_pair_mg;
      poseval_eg[us] += bishop_pair_eg;
    }
  }

  __forceinline void evalKingOneSide(const Side us)
  {
    Square sq = lsb(game_.board.king(us));
    const BB& bbsq = bbSquare(sq);

    int score_mg = king_pcsq_mg[flip[us][sq]];
    int score_eg = king_pcsq_eg[flip[us][sq]];

      score_mg += king_pawn_shelter[popCount((pawnPush[us](bbsq) | pawnWestAttacks[us](bbsq) |
                    pawnEastAttacks[us](bbsq)) & pawns(us))];

    BB eastwest = bbsq | westOne(bbsq) | eastOne(bbsq);

    score_mg += king_on_open[popCount(open_files & eastwest)];
    score_mg += king_on_half_open[popCount(half_open_files[us] & eastwest)];

    if (((us == 0) &&
        (((sq == f1 || sq == g1) && (bbSquare(h1) & game_.board.rooks(0))) ||
        ((sq == c1 || sq == b1) && (bbSquare(a1) & game_.board.rooks(0))))) ||
      ((us == 1) &&
        (((sq == f8 || sq == g8) && (bbSquare(h8) & game_.board.rooks(1))) ||
        ((sq == c8 || sq == b8) && (bbSquare(a8) & game_.board.rooks(1))))))
    {
      score_mg += king_obstructs_rook;
    }
    all_attacks[us] |= king_attacks[kingSq(us)];
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalPassedPawnsOneSide(const Side us)
  {
    const Side them = us ^ 1;

    for (BB files = pawnp ? pawnp->passed_pawn_files[us] : 0; files; resetLSB(files)) {
      for (BB bb = bbFile(lsb(files)) & pawns(us); bb; resetLSB(bb)) {
        int sq = lsb(bb);
        const BB& front_span = pawn_front_span[us][sq];
        int r = us == 0 ? rankOf(sq) : 7 - rankOf(sq);

        int score_mg = passed_pawn_mg[r];
        int score_eg = passed_pawn_eg[r];

        score_eg += passed_pawn_no_us[r]*(front_span & game_.board.occupied_by_side[us] ? 0 : 1);
        score_eg += passed_pawn_no_them[r]*(front_span & game_.board.occupied_by_side[them] ? 0 : 1);
        score_eg += passed_pawn_no_attacks[r]*(front_span & all_attacks[them] ? 0 : 1);
        score_eg += passed_pawn_king_dist_them[distance[sq][kingSq(them)]];
        score_eg += passed_pawn_king_dist_us[distance[sq][kingSq(us)]];

        poseval_mg[us] += score_mg;
        poseval_eg[us] += score_eg;
      }
    }
  }

  __forceinline void evalKingAttackOneSide(const Side side)
  {
    if (attack_count[side] > 1) {
      poseval_mg[side] += attack_counter[side]*(attack_count[side]-1);
    }
  }

  __forceinline const BB& pawns(Side side)
  {
    return game_.board.pawns(side);
  }

  __forceinline Square kingSq(Side side)
  {
    return game_.board.king_square[side];
  }

  __forceinline void initialiseEvaluate()
  {
    pos = game_.pos;
    pos->flags = 0;

    poseval_mg[0] = poseval_eg[0] = poseval[0] = 0;
    poseval_mg[1] = poseval_eg[1] = poseval[1] = 0;

    attack_counter[0] = attack_counter[1] = 0;
    attack_count[0] = attack_count[1] = 0;

    king_area[0] = king_attacks[kingSq(0)] | game_.board.king(0);
    king_area[1] = king_attacks[kingSq(1)] | game_.board.king(1);

    occupied = game_.board.occupied;
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

  void initialise(PawnStructureTable* pawnt)
  {
    this->pawnt = pawnt;
    tuning_ = false;
  }

  Position* pos;
  const Game& game_;
  PawnStructureTable* pawnt;
  PawnEntry* pawnp;

  int poseval_mg[2];
  int poseval_eg[2];
  int poseval[2];
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

public:
  static int knight_mob_mg[9];
  static int knight_mob_eg[9];
  static int knight_mob2_mg[9];
  static int knight_mob2_eg[9];
  static int bishop_mob_mg[14];
  static int bishop_mob_eg[14];
  static int bishop_mob2_mg[14];
  static int bishop_mob2_eg[14];
  static int rook_mob_mg[15];
  static int rook_mob_eg[15];
  static int queen_mob_mg[28];
  static int queen_mob_eg[28];
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
  static int rook_open_file;
  static int rook_half_open_file;
  static int rook_open_file_doubled;
  static int rook_half_open_file_doubled;
  static int knight_in_danger;
  static int bishop_in_danger;
  static int rook_in_danger;
  static int queen_in_danger;
  static int king_pawn_shelter[4];
  static int king_on_open[4];
  static int king_on_half_open[4];
  static int bishop_pair_mg;
  static int bishop_pair_eg;
  static int pawn_isolated_mg[2];
  static int pawn_isolated_eg[2];
  static int pawn_behind_mg[2];
  static int pawn_behind_eg[2];
  static int pawn_doubled_mg[2];
  static int pawn_doubled_eg[2];
  static int passed_pawn_mg[8];
  static int passed_pawn_eg[8];
  static int passed_pawn_no_them[8];
  static int passed_pawn_no_us[8];
  static int passed_pawn_no_attacks[8];
  static int passed_pawn_king_dist_them[8];
  static int passed_pawn_king_dist_us[8];
  static int king_obstructs_rook;
  static int knight_attack_king;
  static int bishop_attack_king;
  static int rook_attack_king;
  static int queen_attack_king;
  bool tuning_;
};

int Eval::bishop_mob2_eg[14] = { -35, -25, -20, -10, -5, 0, 5, 5, 5, 0, 5, 5, 20, -5 };
int Eval::bishop_mob2_mg[14] = { -20, -15, -5, 0, 5, 5, 5, 5, 10, 15, 5, 15, 15, 20 };
int Eval::bishop_mob_eg[14] = { -20, -20, -30, -25, -15, -5, 0, 0, 5, 10, 0, 5, 5, 5 };
int Eval::bishop_mob_mg[14] = { -15, -15, -15, -10, -5, 0, 5, 5, 10, 0, 5, 5, 5, 15 };
int Eval::knight_mob2_eg[9] = { -110, -40, -20, -10, -5, 0, 0, -5, -10 };
int Eval::knight_mob2_mg[9] = { -45, -15, -10, -5, 0, 5, 10, 15, -5 };
int Eval::knight_mob_eg[9] = { -60, -15, -10, -5, -5, -5, -10, -15, -5 };
int Eval::knight_mob_mg[9] = { -15, -15, -5, -5, 0, 0, -5, -5, -15 };

//int Eval::bishop_mob2_eg[14] = { -35, -31, -20, -14, -7, 0, -1, 3, 3, -4, 5, 1, 22, -11 };
//int Eval::bishop_mob2_mg[14] = { -20, -15, -5, 0, 1, 3, 7, 5, 8, 17, 7, 15, 17, 20 };
//int Eval::bishop_mob_eg[14] = { -12, -20, -36, -29, -17, -5, -2, -2, 1, 8, -4, 1, 7, -3 };
//int Eval::bishop_mob_mg[14] = { -15, -15, -15, -8, -5, 0, 3, 5, 8, 2, 5, 5, -5, 19 };
//int Eval::knight_mob2_eg[9] = { -130, -52, -32, -16, -9, 0, 0, -3, -12 };
//int Eval::knight_mob2_mg[9] = { -43, -19, -12, -9, 0, 5, 10, 17, 19 };
//int Eval::knight_mob_eg[9] = { -36, 11, 8, -3, -7, -9, -10, -17, -11 };
//int Eval::knight_mob_mg[9] = { 21, -7, -3, 1, 0, -2, -7, -11, -19 };

int Eval::bishop_attack_king = 6;
int Eval::bishop_in_danger = -47;
int Eval::bishop_pair_eg = 60;
int Eval::bishop_pair_mg = 31;
int Eval::bishop_pcsq_eg[64] = { 5, 8, -4, 4, 5, 0, -25, 7, 1, 5, 4, 7, -1, -9, -3, -12, -12, 8, 0, -13, -4, -4, -5, -13, -6, 1, 1, 11, 10, 4, -1, -8, -17, -6, 5, 10, 2, -2, -7, -20, -25, -17, -7, -2, 4, -11, -18, -25, -41, -32, -25, -13, -18, -28, -26, -63, -46, -37, -25, -23, -18, -13, -42, -34 };
int Eval::bishop_pcsq_mg[64] = { -58, -66, -20, -36, -42, -74, 15, -64, -50, -40, -28, -19, -21, -5, -47, -62, -12, -8, 14, 27, 33, 65, 35, 7, -33, -1, 1, 35, 18, 9, 2, -22, -14, -6, -5, 17, 20, -8, -2, -4, -11, 10, 2, 0, 0, 7, 8, -5, 8, 6, 9, -10, -4, 14, 26, 3, -7, 7, -17, -25, -23, -21, -6, -21 };
int Eval::king_obstructs_rook = -71;
int Eval::king_on_half_open[4] = { 13, -11, -47, -102 };
int Eval::king_on_open[4] = { 31, -13, -74, -207 };
int Eval::king_pawn_shelter[4] = { -18, 1, 6, -4 };
int Eval::king_pcsq_eg[64] = { -116, -33, -29, -32, -27, -18, -17, -119, -46, 20, 25, 13, 19, 32, 42, -55, 3, 31, 37, 30, 28, 40, 43, 4, -16, 13, 30, 35, 33, 34, 23, -1, -29, 3, 21, 24, 30, 25, 9, -17, -29, -5, 10, 17, 20, 20, 4, -21, -24, -6, -1, 8, 10, 11, -3, -25, -45, -38, -15, -4, -32, -16, -33, -65 };
int Eval::king_pcsq_mg[64] = { -70, 93, 108, 133, 125, 65, 36, -22, 2, 50, 33, 43, 40, 21, -23, -57, -54, 50, 9, 9, 7, 16, 18, -45, -11, 47, 21, -13, 2, 3, 28, -66, -30, 24, 2, 12, -18, -14, -7, -62, -40, 15, -19, -18, -29, -35, -10, -36, -10, -3, -11, -42, -41, -32, 7, -2, -31, 27, 9, -69, -14, -31, 30, 8 };
int Eval::knight_attack_king = 7;
int Eval::knight_in_danger = -40;
int Eval::knight_pcsq_eg[64] = { -17, -14, -11, -18, -31, -22, -12, -32, -26, -18, -13, -7, -17, -30, -22, -17, -27, -17, 3, 10, -1, 4, -8, -26, -19, -5, 8, 16, 31, 7, 4, -34, -36, -13, 6, 20, 13, 10, -13, -37, -58, -33, -18, 3, -11, -21, -36, -47, -65, -36, -40, -35, -34, -33, -27, -47, -93, -65, -50, -28, -45, -51, -65, -87 };
int Eval::knight_pcsq_mg[64] = { -252, -100, -68, -18, 36, -37, -107, -246, -65, -51, -8, 30, 44, 36, -26, -52, -32, 15, 36, 47, 81, 82, 33, -29, -21, 0, 41, 40, 14, 60, 11, 11, -24, -4, 14, 9, 22, 9, 15, -15, -43, -12, -10, 12, 22, 5, 9, -34, -53, -45, -26, -5, -5, -11, -36, -24, -62, -31, -53, -51, -15, -27, -33, -75 };
int Eval::passed_pawn_eg[8] = { 0, -14, -7, 3, 14, 14, -111, 0 };
int Eval::passed_pawn_king_dist_them[8] = { 0, -66, -23, 5, 24, 34, 41, 27 };
int Eval::passed_pawn_king_dist_us[8] = { 0, 25, 21, 0, -9, -9, 2, -11 };
int Eval::passed_pawn_mg[8] = { 0, -10, -16, -6, 21, 64, 97, 0 };
int Eval::passed_pawn_no_attacks[8] = { 0, 5, 4, 13, 28, 52, 89, 0 };
int Eval::passed_pawn_no_them[8] = { 0, 3, 6, 23, 37, 67, 131, 0 };
int Eval::passed_pawn_no_us[8] = { 0, 2, -1, 3, 13, 29, 123, 0 };
int Eval::pawn_behind_eg[2] = { 2, -7 };
int Eval::pawn_behind_mg[2] = { -5, -31 };
int Eval::pawn_doubled_eg[2] = { -17, -12 };
int Eval::pawn_doubled_mg[2] = { -16, 3 };
int Eval::pawn_isolated_eg[2] = { -10, -29 };
int Eval::pawn_isolated_mg[2] = { -15, -34 };
int Eval::pawn_pcsq_eg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 3, 35, 49, 31, 24, 60, 50, 27, 37, 39, 32, 18, 15, 18, 27, 17, 26, 25, 15, 4, 6, 6, 19, 16, 19, 16, 7, 6, 3, 6, 10, 9, 9, 8, 11, 10, 13, 7, -2, -2, 15, 9, 17, 20, 21, 16, -2, -5, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::pawn_pcsq_mg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 174, 110, 85, 101, 95, 37, 23, 6, 16, 22, 26, 32, 56, 68, 56, 31, 8, 16, 15, 29, 28, 15, 3, -13, -4, 4, 15, 19, 22, 10, 2, -18, 2, -1, -1, 5, 12, 12, 24, 2, -10, -6, -17, -20, -19, 14, 26, -16, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::queen_attack_king = 39;
int Eval::queen_in_danger = -41;
int Eval::queen_mob_eg[28] = { 1, -24, -19, -21, -39, -36, -26, -23, -20, -26, -16, -7, 0, 1, 2, 4, 5, 6, 3, 1, -8, -21, -26, -39, -28, -38, -23, -45 };
int Eval::queen_mob_mg[28] = { -1, -7, -10, -14, -10, -8, -6, -6, -3, -1, 0, 0, 0, 1, 2, 3, 5, 5, 9, 9, 20, 30, 32, 46, 33, 35, 11, 3 };
int Eval::queen_pcsq_eg[64] = { 0, 14, 9, 25, 29, 3, 4, 9, 14, 37, 40, 37, 51, 35, 36, 13, -23, 6, 37, 36, 36, 29, 10, 11, -9, 13, 25, 45, 51, 35, 36, 10, -37, -8, -1, 27, 13, 4, -4, 4, -50, -47, -14, -37, -15, -26, -16, -43, -74, -59, -65, -58, -54, -95, -111, -65, -89, -85, -80, -62, -81, -94, -116, -105 };
int Eval::queen_pcsq_mg[64] = { -20, -2, 26, 26, 31, 81, 51, 37, -46, -60, -20, 0, -7, 35, -1, 28, -21, -21, -16, -4, 34, 65, 59, 18, -24, -23, -16, -23, -11, 4, 5, -9, -14, -25, -15, -14, -10, -5, 2, -17, -22, -5, -12, -6, -5, 1, 3, -13, -15, -11, 6, 7, 6, 23, 5, -33, 5, -8, -5, 2, 4, -23, -30, -8 };
int Eval::rook_attack_king = 14;
int Eval::rook_in_danger = -39;
int Eval::rook_mob_eg[15] = { -57, -29, -29, -16, -7, -2, 5, 6, 13, 18, 21, 25, 27, 17, 7 };
int Eval::rook_mob_mg[15] = { -27, -21, -19, -17, -18, -12, -10, -3, -1, 2, 6, 8, 8, 21, 31 };
int Eval::rook_open_file = 14;
int Eval::rook_pcsq_eg[64] = { 38, 44, 34, 33, 36, 38, 38, 31, 40, 39, 32, 29, 28, 22, 26, 25, 41, 28, 30, 17, 9, 21, 19, 26, 32, 29, 22, 15, 11, 21, 16, 19, 25, 30, 23, 11, 13, 18, 16, 17, 8, 7, 4, 0, -9, -1, -4, -3, -15, -14, -13, -19, -18, -16, -23, -8, -15, -15, -13, -21, -26, -14, -15, -24 };
int Eval::rook_pcsq_mg[64] = { 12, 7, 32, 34, 30, 44, 37, 51, 1, -6, 29, 42, 44, 50, 29, 37, -17, 13, 15, 36, 63, 72, 48, 8, -22, -14, 4, 15, 21, 21, 11, -5, -47, -43, -29, -20, -17, -17, -13, -35, -50, -36, -31, -25, -7, -19, -11, -36, -48, -32, -14, -9, -8, -5, -4, -71, -15, -10, -2, 4, 11, 3, -11, -4 };
