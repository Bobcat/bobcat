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
  Eval(const Game& game, PawnStructureTable* pawnt) : game_(game) {
    initialise(pawnt);
  }

  virtual ~Eval() {
  }

  virtual void newGame() {
  }

  int evaluate(int alpha, int beta) {
    initialiseEvaluate();

    evalMaterialOneSide(0);
    evalMaterialOneSide(1);

    int mat_eval = poseval[0] - poseval[1];

    int lazy_margin = 300;
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
    int score_mg = 0;
    int score_eg = 0;

    for (BB bb = pawns(us); bb; ) {
      Square sq = lsb(bb);

      if (game_.board.isPawnPassed(sq, us)) {
        passed_pawn_files[us] |= 1 << file(sq);
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

  __forceinline void evalKnightsOneSide(const Side us) {
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

      all_attacks[us] |= attacks;
      _knight_attacks[us] |= attacks;

      bool outpost = (passed_pawn_front_span[us][sq] & (pawns(them) & ~pawn_front_span[us][sq])) == 0;

      if (outpost && (pawn_attacks[us] & bbSquare(sq))) {
        score += std::max(0, knight_pcsq_eg[flipsq]);
      }

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

  __forceinline void evalBishopsOneSide(const Side us) {
    const Side them = us ^ 1;
    int score_mg = 0;
    int score_eg = 0;
    int score = 0;

    for (BB bishops = game_.board.bishops(us); bishops; resetLSB(bishops)) {
      Square sq = lsb(bishops);
      Square flipsq = flip[us][sq];

      score_mg += bishop_pcsq_mg[flipsq];
      score_eg += bishop_pcsq_eg[flipsq];

      const BB attacks = Bmagic(sq, occupied);
      int x = popCount(attacks & ~(game_.board.occupied_by_side[us]));

      score_mg += bishop_mob_mg[x];
      score_eg += bishop_mob_eg[x];

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

  __forceinline void evalRooksOneSide(const Side us) {
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

//      if ((bbsq & rank_7[us]) && (rank_7_and_8[us] & (pawns(them) | game_.board.king(them)))) {
//        score += rook_seventh_rank;
//      }
      const BB attacks = Rmagic(sq, occupied);
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

  __forceinline void evalQueensOneSide(const Side us) {
    const Side them = us ^ 1;
    int score_mg = 0;
    int score_eg = 0;
    int score = 0;

    for (BB queens = game_.board.queens(us); queens; resetLSB(queens)) {
      Square sq = lsb(queens);
      Square flipsq = flip[us][sq];

      score_mg += queen_pcsq_mg[flipsq];
      score_eg += queen_pcsq_eg[flipsq];

      const BB attacks = Qmagic(sq, occupied);

      all_attacks[us] |= attacks;
      queen_attacks[us] |= attacks;

      if (attacks & king_area[them]) {
        attack_counter[us] += popCount(attacks & king_area[them])*queen_attack_king;
        attack_count[us]++;
      }

      if (bbSquare(sq) & (pawn_attacks[them] | _knight_attacks[them] | bishop_attacks[them] | rook_attacks[them])) {
        score += queen_in_danger;
      }
//      else {
//        score += queen_approach_king*(7 - distance[sq][kingSq(them)]);
//      }
    }
    poseval[us] += score;
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalMaterialOneSide(const Side us) {
    poseval[us] = pos->material.material_value[us];

    if (pos->material.count(us, Bishop) == 2) {
      poseval_mg[us] += bishop_pair_mg;
      poseval_eg[us] += bishop_pair_eg;
    }
  }

  __forceinline void evalKingOneSide(const Side us) {
 //  const Side them = us ^ 1;
    Square sq = lsb(game_.board.king(us));
    const BB& bbsq = bbSquare(sq);

    int score_mg = king_pcsq_mg[flip[us][sq]];
    int score_eg = king_pcsq_eg[flip[us][sq]];

    score_mg += king_pawn_shelter[popCount((pawnPush[us](bbsq) | pawnWestAttacks[us](bbsq) |
                  pawnEastAttacks[us](bbsq)) & pawns(us))];

    //int majors = std::min(popCount(game_.board.rooks(them)) + popCount(game_.board.queens(them))*2, 4);
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

  __forceinline void evalPassedPawnsOneSide(const Side us) {
    const Side them = us ^ 1;

    for (BB files = pawnp ? pawnp->passed_pawn_files[us] : 0; files; resetLSB(files)) {
      for (BB bb = bbFile(lsb(files)) & pawns(us); bb; resetLSB(bb)) {
        int sq = lsb(bb);
        const BB& front_span = pawn_front_span[us][sq];
        int r = us == 0 ? rank(sq) : 7 - rank(sq);

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

  __forceinline void evalKingAttackOneSide(const Side side) {
    if (attack_count[side] > 1) {
      poseval_mg[side] += attack_counter[side]*(attack_count[side]-1);
    }
  }

  __forceinline const BB& pawns(Side side) {
    return game_.board.pawns(side);
  }

  __forceinline Square kingSq(Side side) {
    return game_.board.king_square[side];
  }

  __forceinline void initialiseEvaluate() {
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

  void initialise(PawnStructureTable* pawnt) {
    this->pawnt = pawnt;
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
  static int bishop_mob_mg[14];
  static int bishop_mob_eg[14];
  static int rook_mob_mg[15];
  static int rook_mob_eg[15];
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
//  static int queen_approach_king;
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
};

int Eval::bishop_in_danger = -45;
int Eval::bishop_mob_eg[14] = { -50, -45, -45, -30, -20, -10, 0, 5, 10, 10, 10, 15, 10, 20 };
int Eval::bishop_mob_mg[14] = { -35, -20, -10, -10, -5, 10, 10, 10, 10, 15, 15, 20, 10, 30 };
int Eval::bishop_pair_eg = 45;
int Eval::bishop_pair_mg = 20;
int Eval::bishop_pcsq_eg[64] = { -15, -10, 5, 0, 0, -15, -10, -5, -15, 20, 15, 15, 5, -5, 0, -15, -10, 10, 15, 5, 15, 15, 5, 5, -10, 10, 10, 25, 20, 15, 10, 0, -10, 10, 15, 20, 15, -5, 5, -25, -20, -5, 0, 10, 15, 5, -20, -10, -35, -25, -15, -5, -5, -20, 0, -45, -35, -20, -20, -20, -20, -15, -35, -25 };
int Eval::bishop_pcsq_mg[64] = { -30, -60, 5, 0, -10, -60, -5, -20, -45, -35, -25, 10, 5, 10, 5, -45, -15, -15, 25, 15, 30, 45, 20, 10, -20, 5, 10, 40, 25, 15, 10, 5, -15, 5, -5, 20, 25, 5, 5, -5, -5, 15, 10, -5, 0, 5, 5, -5, 5, 15, 5, -5, -5, -10, 25, -10, -20, 0, -20, -20, -20, -20, -20, -20 };
int Eval::king_pcsq_eg[64] = { -97, -20, -24, -29, -20, -11, -14, -141, -24, 20, 38, 11, 15, 35, 49, -44, 4, 33, 36, 28, 31, 44, 46, 0, -4, 15, 26, 28, 27, 31, 25, 0, -23, 1, 18, 16, 21, 18, 11, -17, -26, -9, 2, 8, 10, 10, 1, -16, -16, -2, -5, 4, 4, 2, -2, -19, -35, -35, -10, -4, -15, -14, -26, -40 };
int Eval::king_pcsq_mg[64] = { -72, 85, 82, 97, 110, 42, 22, -156, 3, 60, 35, 38, 38, 25, -18, -38, -8, 47, 20, 16, 5, 23, 23, -24, -4, 50, 24, 2, 8, 12, 31, -18, -19, 37, -6, 14, -6, -11, -1, -27, -21, 28, 8, -1, -12, -12, -2, -26, 1, 6, 9, -26, -21, -9, 6, -6, -22, 27, 4, -48, -10, -23, 13, -3 };
int Eval::knight_in_danger = -40;
int Eval::knight_mob_eg[9] = { -45, -40, -35, -25, -20, -10, -5, 5, 0 };
int Eval::knight_mob_mg[9] = { -30, -25, -25, -15, -5, 0, -5, 0, 5 };
int Eval::pawn_behind_eg[2] = { 10, 10 };
int Eval::pawn_behind_mg[2] = { -5, -25 };
int Eval::pawn_doubled_eg[2] = { -10, -15 };
int Eval::pawn_doubled_mg[2] = { -10, -10 };
int Eval::pawn_isolated_eg[2] = { -10, -10 };
int Eval::pawn_isolated_mg[2] = { -10, -35 };
int Eval::pawn_pcsq_eg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 15, 50, 45, 30, 30, 65, 50, 40, 35, 30, 30, 25, 25, 30, 30, 25, 15, 15, 10, 0, 10, 10, 20, 15, 0, 5, 5, -5, -5, 5, 10, -10, -10, 0, 0, -5, 5, -5, -10, -15, -10, 5, -5, -10, -5, 10, 5, -15, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::pawn_pcsq_mg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 125, 80, 55, 75, 75, 25, 55, 40, 25, 25, 25, 25, 45, 65, 40, 25, -5, 10, 10, 20, 20, 5, -5, -10, -10, 0, 5, 10, 10, -10, -10, -15, 0, -5, -5, -10, -5, -5, 20, 5, -10, -5, -10, -20, -30, 10, 15, -15, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::queen_in_danger = -40;
int Eval::rook_in_danger = -40;
int Eval::rook_mob_eg[15] = { -50, -20, -25, -20, -15, -15, -10, -5, -5, 5, 10, 15, 15, 10, 15 };
int Eval::rook_mob_mg[15] = { -30, -25, -20, -15, -15, -10, -5, 0, 10, 10, 15, 15, 15, 20, 15 };
int Eval::rook_open_file = 20;
int Eval::rook_pcsq_eg[64] = { 25, 25, 20, 25, 20, 30, 30, 25, 35, 25, 30, 30, 30, 25, 20, 25, 30, 25, 25, 15, 15, 25, 15, 15, 10, 15, 15, 10, 10, 15, 5, 5, -5, 5, 10, 5, 5, 0, 0, -5, -15, -10, -10, -5, -15, -10, -15, -15, -25, -15, -10, -15, -15, -15, -20, -25, -15, -10, -15, -25, -25, -20, -10, -20 };
int Eval::rook_pcsq_mg[64] = { 20, 20, 40, 25, 35, 40, 35, 40, -5, 15, 25, 30, 35, 35, 20, 35, -15, 5, 5, 30, 40, 50, 30, 15, -10, -10, 10, 20, 20, 25, 10, 5, -30, -25, -15, 5, 5, 10, -5, 5, -35, -20, -10, -15, 0, -10, -10, -25, -35, -20, -5, -10, -10, -10, -15, -55, -15, -10, 5, 10, 10, 10, -10, -15 };
int Eval::bishop_attack_king = 4;
int Eval::king_obstructs_rook = -62;
int Eval::king_on_half_open[4] = { 19, -12, -51, -100 };
int Eval::king_on_open[4] = { 52, 5, -53, -166 };
int Eval::king_pawn_shelter[4] = { -21, -1, 5, -1 };
int Eval::knight_attack_king = 6;
int Eval::passed_pawn_eg[8] = { 0, -6, -2, -1, 17, 24, -8, 0 };
int Eval::passed_pawn_king_dist_them[8] = { 0, -67, -22, 7, 25, 33, 40, 36 };
int Eval::passed_pawn_king_dist_us[8] = { 0, 33, 27, 8, 0, -1, 12, -8 };
int Eval::passed_pawn_mg[8] = { 0, -3, -16, -3, 15, 40, 80, 0 };
int Eval::passed_pawn_no_attacks[8] = { 0, 10, 6, 12, 32, 54, 68, 0 };
int Eval::passed_pawn_no_them[8] = { 0, -4, 0, 25, 30, 54, 90, 0 };
int Eval::passed_pawn_no_us[8] = { 0, -25, -10, 2, 7, 21, 50, 0 };
int Eval::queen_attack_king = 38;
int Eval::rook_attack_king = 13;
/*
int Eval::knight_pcsq_eg[64] = {
   -3,  -10,    0,  -13,  -11,  -18,   -9,  -43,
  -23,  -17,   -7,   -4,  -11,  -22,  -27,  -22,
  -15,   -5,    6,   11,    7,    7,    8,  -18,
   -9,    6,    9,   16,   22,    7,   13,  -20,
  -35,   -8,    7,   20,   13,    3,  -10,  -25,
  -40,  -32,  -24,   -6,   -6,  -26,  -20,  -35,
  -46,  -26,  -30,  -30,  -20,  -32,  -13,  -39,
  -66,  -44,  -49,  -22,  -35,  -30,  -49,  -80 };
int Eval::knight_pcsq_mg[64] = {
 -220,  -63,  -69,  -10,   44,  -22,  -95, -233,
  -56,  -29,  -13,   28,   52,   27,  -26,  -48,
  -25,   11,   11,   18,   42,   42,   23,  -18,
  -15,   -1,   25,   27,    6,   35,    2,    8,
  -15,    0,    8,   -2,   18,   13,    5,   -9,
  -32,  -14,  -14,    9,   14,   -2,    1,  -30,
  -45,  -40,  -16,   -3,   -4,   -3,  -26,  -15,
  -55,  -19,  -43,  -42,  -11,  -19,  -31,  -92 };
int Eval::queen_pcsq_eg[64] = {
   -2,    4,   25,   32,   38,   27,    9,   18,
    9,   19,   47,   33,   45,   55,   42,    7,
   -9,   19,   38,   44,   50,   37,   22,    5,
   -4,   20,   25,   35,   45,   37,   40,    4,
  -17,   -8,    3,   20,   13,    0,    6,    4,
  -35,  -24,   -7,  -27,   -4,  -20,  -10,  -42,
  -48,  -30,  -49,  -30,  -31,  -68,  -50,  -28,
  -47,  -60,  -60,  -55,  -45,  -70,  -86,  -67 };
int Eval::queen_pcsq_mg[64] = {
    4,   25,   39,   27,   42,   73,   44,   39,
  -29,  -28,   -5,    7,   20,   40,   37,   32,
  -18,  -16,    4,   25,   52,   67,   71,   18,
  -13,  -12,    7,    0,   20,   22,   20,   -6,
  -15,  -15,   -1,    0,    2,    6,    4,  -13,
  -25,   -8,   -1,   -2,    1,    3,    5,   -8,
  -21,  -20,    4,    2,    0,   10,  -16,  -28,
  -14,  -17,  -17,   -3,  -10,  -21,  -40,  -20 };
New E:0.0830266
*/
int Eval::knight_pcsq_eg[64] = { -3, -10, 0, -13, -11, -18, -9, -43, -23, -17, -7, -4, -11, -22, -27, -22, -15, -5, 6, 11, 7, 7, 8, -18, -9, 6, 9, 16, 22, 7, 13, -20, -35, -8, 7, 20, 13, 3, -10, -25, -40, -32, -24, -6, -6, -26, -20, -35, -46, -26, -30, -30, -20, -32, -13, -39, -66, -44, -49, -22, -35, -30, -49, -80 };
int Eval::knight_pcsq_mg[64] = { -220, -63, -69, -10, 44, -22, -95, -233, -56, -29, -13, 28, 52, 27, -26, -48, -25, 11, 11, 18, 42, 42, 23, -18, -15, -1, 25, 27, 6, 35, 2, 8, -15, 0, 8, -2, 18, 13, 5, -9, -32, -14, -14, 9, 14, -2, 1, -30, -45, -40, -16, -3, -4, -3, -26, -15, -55, -19, -43, -42, -11, -19, -31, -92 };
int Eval::queen_pcsq_eg[64] = { -2, 4, 25, 32, 38, 27, 9, 18, 9, 19, 47, 33, 45, 55, 42, 7, -9, 19, 38, 44, 50, 37, 22, 5, -4, 20, 25, 35, 45, 37, 40, 4, -17, -8, 3, 20, 13, 0, 6, 4, -35, -24, -7, -27, -4, -20, -10, -42, -48, -30, -49, -30, -31, -68, -50, -28, -47, -60, -60, -55, -45, -70, -86, -67 };
int Eval::queen_pcsq_mg[64] = { 4, 25, 39, 27, 42, 73, 44, 39, -29, -28, -5, 7, 20, 40, 37, 32, -18, -16, 4, 25, 52, 67, 71, 18, -13, -12, 7, 0, 20, 22, 20, -6, -15, -15, -1, 0, 2, 6, 4, -13, -25, -8, -1, -2, 1, 3, 5, -8, -21, -20, 4, 2, 0, 10, -16, -28, -14, -17, -17, -3, -10, -21, -40, -20 };

