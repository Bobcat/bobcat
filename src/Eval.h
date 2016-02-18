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
};

int Eval::bishop_attack_king = 6;
int Eval::bishop_in_danger = -45;
int Eval::bishop_mob_eg[14] = { -58, -59, -61, -38, -28, -20, -12, -3, 0, 4, -2, 7, 0, 10 };
int Eval::bishop_mob_mg[14] = { -37, -24, -12, -10, -3, 6, 10, 8, 8, 9, 15, 18, 0, 26 };
int Eval::bishop_pair_eg = 61;
int Eval::bishop_pair_mg = 28;
int Eval::bishop_pcsq_eg[64] = {
   -9,    4,   -3,    2,    4,   -7,  -18,    5,
   -5,   10,    7,    9,   -5,  -11,   -6,  -23,
  -16,    4,    5,  -15,    1,   -1,    1,  -11,
   -6,    4,    2,   17,   10,    7,    2,   -6,
  -16,    0,    5,   14,    5,   -5,   -3,  -19,
  -24,  -13,   -6,    2,    5,   -7,  -22,  -26,
  -39,  -27,  -25,  -13,  -15,  -26,  -20,  -57,
  -43,  -22,  -26,  -20,  -14,  -11,  -43,  -23 };
int Eval::bishop_pcsq_mg[64] = {
  -50,  -70,    5,  -10,  -26,  -66,   15,  -40,
  -51,  -41,  -31,    0,  -11,   -4,  -15,  -61,
  -17,  -25,   19,   23,   28,   57,   26,   12,
  -32,   -3,    0,   34,   23,    9,    4,  -15,
  -15,   -7,   -7,   16,   23,   -3,   -1,  -11,
  -11,    7,    4,   -3,    0,    3,    5,   -9,
   -5,    9,    3,   -9,   -7,    6,   25,    2,
   -8,    0,  -18,  -26,  -20,  -20,    0,  -16 };
int Eval::king_obstructs_rook = -74;
int Eval::king_on_half_open[4] = { 17, -12, -53, -116 };
int Eval::king_on_open[4] = { 52, 7, -57, -198 };
int Eval::king_pawn_shelter[4] = { -19, 1, 5, -5 };
int Eval::king_pcsq_eg[64] = {
 -109,  -30,  -22,  -29,  -18,  -11,  -14, -141,
  -38,   18,   26,   15,   15,   35,   43,  -58,
    0,   27,   38,   32,   31,   44,   44,   -6,
  -12,   17,   30,   34,   33,   33,   23,   -6,
  -25,    5,   24,   24,   27,   22,   11,  -17,
  -32,   -3,    6,   16,   18,   18,    3,  -20,
  -28,    0,   -1,    8,   10,   10,   -2,  -25,
  -47,  -39,  -16,   -4,  -31,  -18,  -34,  -62 };
int Eval::king_pcsq_mg[64] = {
  -66,   77,  104,  135,  120,   62,   36,  -24,
    7,   50,   29,   26,   40,   19,  -24,  -50,
  -56,   53,   14,   14,    7,   19,   21,  -42,
   -8,   48,   26,   -2,   10,    8,   31,  -48,
  -29,   35,    0,   18,  -20,  -15,   -5,  -51,
  -29,   18,   -4,   -7,  -28,  -32,   -6,  -28,
    3,   -2,   -1,  -36,  -37,  -25,    6,   -2,
  -30,   27,    4,  -58,  -12,  -21,   25,    3 };
int Eval::knight_attack_king = 6;
int Eval::knight_in_danger = -40;
int Eval::knight_mob_eg[9] = { -43, -50, -45, -29, -22, -16, -17, -19, -24 };
int Eval::knight_mob_mg[9] = { -28, -17, -15, -7, 1, 0, -3, -2, -1 };
int Eval::knight_pcsq_eg[64] = {
   -9,  -20,  -10,  -15,  -21,  -28,  -13,  -41,
  -25,  -21,  -19,   -6,  -15,  -26,  -35,  -30,
  -23,  -11,   10,   13,    9,   11,    2,  -20,
  -15,    2,    9,   18,   26,    5,    9,  -32,
  -43,  -16,   11,   18,   13,    7,  -16,  -31,
  -52,  -36,  -22,    2,  -10,  -20,  -34,  -43,
  -56,  -28,  -36,  -34,  -34,  -30,  -25,  -39,
  -82,  -46,  -49,  -18,  -43,  -42,  -59,  -78 };
int Eval::knight_pcsq_mg[64] = {
 -220,  -73,  -75,  -14,   36,  -26, -105, -243,
  -66,  -39,   -9,   20,   46,   37,  -32,  -60,
  -33,   15,   25,   36,   58,   48,   25,  -30,
  -19,   -1,   37,   35,   12,   55,    2,    6,
  -21,   -8,    8,    6,   20,   11,    1,  -13,
  -40,  -12,   -8,    7,   24,    2,    7,  -34,
  -55,  -50,  -20,   -3,   -2,  -15,  -28,  -25,
  -55,  -25,  -59,  -50,   -9,  -17,  -33,  -84 };
int Eval::passed_pawn_eg[8] = { 0, -8, -4, 1, 17, 20, -106, 0 };
int Eval::passed_pawn_king_dist_them[8] = { 0, -65, -22, 7, 25, 35, 40, 28 };
int Eval::passed_pawn_king_dist_us[8] = { 0, 29, 23, 4, -6, -7, 4, -12 };
int Eval::passed_pawn_mg[8] = { 0, -13, -18, -9, 17, 56, 88, 0 };
int Eval::passed_pawn_no_attacks[8] = { 0, 4, 4, 10, 26, 52, 82, 0 };
int Eval::passed_pawn_no_them[8] = { 0, -4, 2, 25, 38, 64, 124, 0 };
int Eval::passed_pawn_no_us[8] = { 0, -3, -4, 2, 9, 21, 122, 0 };
int Eval::pawn_behind_eg[2] = { 4, 0 };
int Eval::pawn_behind_mg[2] = { -5, -31 };
int Eval::pawn_doubled_eg[2] = { -10, -19 };
int Eval::pawn_doubled_mg[2] = { -12, 10 };
int Eval::pawn_isolated_eg[2] = { -14, -18 };
int Eval::pawn_isolated_mg[2] = { -14, -37 };
int Eval::pawn_pcsq_eg[64] = {
    0,    0,    0,    0,    0,    0,    0,    0,
   19,   52,   47,   32,   32,   67,   52,   38,
   39,   34,   28,   25,   23,   20,   34,   25,
   23,   21,    8,    0,    4,    6,   20,   19,
   16,   11,    7,   -1,   -3,    7,   10,    2,
    4,    2,    6,    1,    7,    5,   -4,   -5,
    8,    5,    7,   10,   19,   12,   -1,   -9,
    0,    0,    0,    0,    0,    0,    0,    0 };
int Eval::pawn_pcsq_mg[64] = {
    0,    0,    0,    0,    0,    0,    0,    0,
  135,  110,   57,   77,   77,   27,   57,   38,
   17,   15,   21,   29,   43,   59,   52,   29,
    1,   12,   14,   24,   22,    3,    1,  -16,
   -6,    2,   11,   16,   14,    4,   -6,  -11,
    0,   -1,   -3,    2,    7,    5,   18,    3,
   -6,   -5,  -14,  -18,  -22,   10,   21,  -11,
    0,    0,    0,    0,    0,    0,    0,    0 };
int Eval::queen_attack_king = 42;
int Eval::queen_in_danger = -42;
int Eval::queen_mob_eg[28] = { 2, -8, -8, -8, -8, -8, -8, -4, -8, -8, -8, -4, 4, 0, 2, 4, 6, 4, 4, 6, 2, -6, -8, -8, -8, -8, -2, -8 };
int Eval::queen_mob_mg[28] = { -4, -6, -6, -8, -6, -4, -4, -2, 0, -2, 2, 2, 0, 2, 4, 4, 4, 4, 4, 6, 8, 8, 8, 8, 8, 2, 8, -8 };
int Eval::queen_pcsq_eg[64] = {
   -2,   16,   17,   28,   36,   19,    5,   18,
   15,   33,   43,   33,   45,   43,   32,    7,
  -21,   15,   40,   50,   46,   31,   16,    3,
   -8,   20,   29,   43,   47,   39,   40,    8,
  -29,   -6,   -1,   30,   17,    4,    4,    4,
  -53,  -46,   -9,  -27,   -8,  -20,  -16,  -46,
  -68,  -56,  -61,  -44,  -39,  -84,  -86,  -42,
  -75,  -62,  -76,  -57,  -75,  -78,  -88,  -87 };
int Eval::queen_pcsq_mg[64] = {
   -8,    1,   31,   29,   32,   85,   42,   35,
  -43,  -44,  -11,    7,   10,   44,   29,   20,
  -14,  -20,  -10,    5,   46,   63,   61,   24,
  -23,  -14,   -7,  -12,    6,   12,   14,   -8,
  -15,  -19,   -5,   -8,   -2,   -2,    2,  -15,
  -25,   -4,   -3,   -2,    1,    1,    7,  -16,
  -17,  -10,    6,    4,    4,   16,   -6,  -30,
    2,  -17,   -9,   -1,    0,  -23,  -46,   -8 };
int Eval::rook_attack_king = 13;
int Eval::rook_in_danger = -38;
int Eval::rook_mob_eg[15] = { -54, -26, -31, -18, -7, -5, -2, 1, 9, 13, 20, 23, 23, 18, 9 };
int Eval::rook_mob_mg[15] = { -28, -21, -16, -15, -17, -10, -7, 2, 2, 6, 7, 11, 11, 24, 29 };
int Eval::rook_open_file = 16;
int Eval::rook_pcsq_eg[64] = {
   31,   37,   28,   31,   32,   34,   36,   29,
   37,   33,   34,   32,   28,   23,   24,   27,
   38,   29,   27,   17,    9,   23,   19,   21,
   28,   25,   19,   12,   10,   21,   11,   13,
   17,   23,   22,    3,    7,    8,   10,   11,
    1,   -6,    2,   -1,   -9,   -2,   -9,   -7,
  -17,  -13,   -8,  -19,  -15,  -19,  -28,   -7,
  -15,  -10,  -15,  -21,  -23,  -16,  -16,  -18 };
int Eval::rook_pcsq_mg[64] = {
   22,    6,   44,   35,   37,   52,   41,   46,
   -1,    1,   33,   32,   41,   55,   24,   33,
  -17,    9,   11,   36,   52,   58,   32,    7,
  -18,  -10,    6,   24,   24,   27,    4,    1,
  -40,  -33,  -23,  -13,   -9,   -2,   -7,  -27,
  -43,  -22,  -22,  -21,    2,  -12,  -12,  -31,
  -41,  -24,   -9,   -6,   -2,    2,   -1,  -71,
  -11,   -8,    3,    8,   10,    8,   -4,   -7 };
