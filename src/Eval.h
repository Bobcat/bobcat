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
      bool open_file = !game_.board.isPieceOnFile(Pawn, sq, us^1);

      if (game_.board.isPawnIsolated(sq, us)) {
        score_mg += open_file ? -40 : -20;
        score_eg += -20;
      }
      else if (game_.board.isPawnBehind(sq, us)) {
        score_mg += open_file ? -20 : -8;
        score_eg += -8;

        if ((pawns(us^1) | pawn_attacks[us^1]) & pawnPush[us](bbSquare(sq))) {
          score_mg += open_file ? -20 : -8;
          score_eg += -8;
        }
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
        attack_counter[us] += popCount(attacks & king_area[them])*8;
        attack_count[us]++;
      }

      if (bbSquare(sq) & pawn_attacks[them]) {
        score += -28;
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
        attack_counter[us] += popCount(attacks & king_area[them])*6;
        attack_count[us]++;
      }

      if (bbSquare(sq) & pawn_attacks[them]) {
        score += -28;
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
      //Square flipsq = flip[us][sq];

      //score_mg += rook_pcsq_mg[flipsq];
      //score_eg += rook_pcsq_eg[flipsq];

      const BB& bbsq = bbSquare(sq);

      if (bbsq & open_files) {
        score += 20;

        if (~bbsq & bbFile(sq) & game_.board.rooks(us)) {
          score += 20;
        }
      }
      else if (bbsq & half_open_files[us]) {
        score += 10;

        if (~bbsq & bbFile(sq) & game_.board.rooks(us)) {
          score += 10;
        }
      }

      if ((bbsq & rank_7[us]) && (rank_7_and_8[us] & (pawns(them) | game_.board.king(them)))) {
        score += 20;
      }
      const BB attacks = Rmagic(sq, occupied);
      int x = popCount(attacks & ~game_.board.occupied_by_side[us]);

      score_mg += rook_mob_mg[x];
      score_eg += rook_mob_eg[x];

      all_attacks[us] |= attacks;
      rook_attacks[us] |= attacks;

      if (attacks & king_area[them]) {
        attack_counter[us] += popCount(attacks & king_area[them])*12;
        attack_count[us]++;
      }

      if (bbSquare(sq) & (pawn_attacks[them] | _knight_attacks[them] | bishop_attacks[them])) {
        score += -36;
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

      const BB& bbsq = bbSquare(sq);

      if ((bbsq & rank_7[us]) && (rank_7_and_8[us] & (pawns(them) | game_.board.king(them)))) {
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
        score += -40;
      }
      else {
        score += 6*(7 - distance[sq][kingSq(them)]);
      }
    }
    poseval[us] += score;
    poseval_mg[us] += score_mg;
    poseval_eg[us] += score_eg;
  }

  __forceinline void evalMaterialOneSide(const Side us) {
    poseval[us] = pos->material.material_value[us];

    if (pos->material.count(us, Bishop) == 2) {
      poseval[us] += 30;

      if (pos->material.pawnCount() <= 12) {
        poseval[us] += 20;
      }
    }
  }

  __forceinline void evalKingOneSide(const Side us) {
    const Side them = us ^ 1;
    Square sq = lsb(game_.board.king(us));
    const BB& bbsq = bbSquare(sq);

    int score_mg = king_pcsq_mg[flip[us][sq]];
    int score_eg = king_pcsq_eg[flip[us][sq]];

    score_mg += -45 + 15*popCount((pawnPush[us](bbsq) | pawnWestAttacks[us](bbsq) |
      pawnEastAttacks[us](bbsq)) & pawns(us));

    if (game_.board.queens(them) || popCount(game_.board.rooks(them)) > 1) {
      BB eastwest = bbsq | westOne(bbsq) | eastOne(bbsq);
      int x = -15*popCount(open_files & eastwest);
      int y = -10*popCount(half_open_files[us] & eastwest);

      score_mg += x;
      score_mg += y;

      if (game_.board.queens(them) && popCount(game_.board.rooks(them))) {
        score_mg += x;
        score_mg += y;

        if (popCount(game_.board.rooks(them) > 1)) {
          score_mg += x;
          score_mg += y;
        }
      }
    }

    if (((us == 0) &&
        (((sq == f1 || sq == g1) && (bbSquare(h1) & game_.board.rooks(0))) ||
        ((sq == c1 || sq == b1) && (bbSquare(a1) & game_.board.rooks(0))))) ||
      ((us == 1) &&
        (((sq == f8 || sq == g8) && (bbSquare(h8) & game_.board.rooks(1))) ||
        ((sq == c8 || sq == b8) && (bbSquare(a8) & game_.board.rooks(1))))))
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

        score_eg += rr*(front_span & game_.board.occupied_by_side[us] ? 0 : 1);
        score_eg += rr*(front_span & game_.board.occupied_by_side[them] ? 0 : 1);
        score_eg += rr*(front_span & all_attacks[them] ? 0 : 1);
        score_eg += r*(distance[sq][kingSq(them)]*2-distance[sq][kingSq(us)]*2);

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
  static int passed_pawn_mg[8];
  static int passed_pawn_eg[8];
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
};

int Eval::bishop_mob_eg[14] = { -45, -35, -30, -20, -10, 0, 5, 20, 20, 20, 20, 25, 25, 30 };
int Eval::bishop_mob_mg[14] = { -50, -25, -20, -10, -5, 10, 15, 15, 20, 20, 20, 30, 35, 50 };
int Eval::bishop_pcsq_eg[64] = { 0, -35, 0, 20, -10, -30, 0, 30, -15, 10, 5, 20, -35, 5, 15, 15, -15, 5, 0, -20, -5, 15, 10, 0, -10, 5, -10, 15, -10, 10, 0, 5, -15, -5, 0, 5, 0, -10, 0, -25, -40, 5, -5, 0, 0, 0, -25, -15, -10, -30, -15, -15, -20, -40, -25, -50, -35, -10, -30, -25, -25, -35, -30, -55 };
int Eval::bishop_pcsq_mg[64] = { -15, -50, 10, -20, 0, -25, 75, 0, -35, -25, 15, -45, 40, 5, -5, 10, -25, -50, 5, -10, 10, 45, 20, -15, -20, 0, -5, 20, 40, 15, 0, 10, -35, -5, -5, 10, 20, -10, -15, -5, -20, 5, 5, -5, -10, -10, -10, -15, -15, 10, -10, -15, -15, -15, 20, -30, -35, -10, -30, -30, -25, -20, -30, -15 };
int Eval::king_pcsq_eg[64] = { -42, -25, -10, -5, -5, -10, -25, -42, -22, -5, 6, 11, 11, 6, -5, -22, -15, 2, 16, 21, 21, 16, 2, -15, -17, 8, 25, 30, 30, 25, 8, -17, -23, 2, 20, 25, 25, 20, 2, -23, -28, -3, 11, 16, 16, 11, -3, -28, -35, -10, 1, 6, 6, 1, -10, -35, -70, -45, -30, -25, -25, -30, -45, -70 };
int Eval::king_pcsq_mg[64] = { 5, 10, -20, -40, -40, -19, 10, 5, 14, 19, -10, -29, -29, -8, 19, 14, 22, 27, 0, -19, -19, 0, 27, 22, 29, 34, 8, -14, -14, 7, 34, 29, 34, 39, 14, -9, -9, 12, 39, 34, 36, 41, 16, -6, -6, 15, 41, 36, 38, 43, 18, -3, -3, 17, 43, 38, 40, 45, 20, 0, 0, 20, 45, 40 };
int Eval::knight_mob_eg[9] = { 0, -25, -25, -20, -5, -15, -15, 0, -10 };
int Eval::knight_mob_mg[9] = { -30, -25, -25, -20, -10, 0, -5, -5, 0 };
int Eval::knight_pcsq_eg[64] = { -55, -40, -45, 0, 0, -5, 0, -95, -40, 45, -10, 10, 25, 15, -25, -30, -25, 5, 15, 15, 20, 20, 15, -15, 10, 15, 15, 25, 30, 25, 15, 35, -15, 15, 20, 15, 20, 15, -10, 10, -25, -15, -5, 0, 20, -15, -5, -20, -35, -30, -15, -20, -10, -25, -10, -40, -85, -30, -50, -35, -45, -25, -50, -105 };
int Eval::knight_pcsq_mg[64] = { -65, -110, -95, 5, 65, -65, 85, -105, -70, -50, 0, 60, 45, 30, -20, -40, -40, 15, 10, 30, 50, 45, 30, 25, 5, 10, 35, 45, 30, 30, 15, -10, -15, -10, 5, 20, 20, 25, 0, 5, -35, -15, -5, 5, 15, 10, 0, -35, -35, -40, -20, 0, 0, -15, -15, -40, -35, -40, -65, -35, -35, -15, -40, -150 };
int Eval::pawn_pcsq_eg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 75, 85, 50, 30, 50, 50, 50, 70, 45, 30, 35, 15, 0, 50, 45, 35, 20, 20, 0, 0, 5, 0, 0, 15, 5, 10, 5, 0, 0, 15, 0, 0, 0, -5, 5, 10, 5, 15, 0, 0, 0, 0, -5, -5, 20, 15, 10, -10, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::pawn_pcsq_mg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 70, 60, 20, 30, 30, 20, 10, 40, 10, -10, 10, 40, 30, 30, 30, 30, -5, 5, 10, 20, 25, -5, -15, -10, -5, 0, 10, 10, 10, 0, -10, -10, 0, -5, 0, 0, -5, -5, -5, 0, 0, 5, -5, -10, -10, 10, 15, -5, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::queen_pcsq_eg[64] = { 20, 30, 25, 45, 45, 30, 60, 10, -5, -5, 40, 50, 35, 50, 45, 30, -5, 15, 35, 35, 55, 55, 60, 0, 20, 45, 35, 30, 45, 25, 30, 10, -10, -5, 5, 15, 5, 10, 15, 30, -15, -15, 5, -15, 0, -10, 0, -15, -20, -30, -10, -20, -10, -25, -25, -25, -15, -30, -25, -15, -25, -20, -30, -40 };
int Eval::queen_pcsq_mg[64] = { -10, -20, 10, 35, 50, 70, 70, 45, -15, -35, -25, -20, 25, 45, 5, 40, -10, 0, 15, 20, 50, 50, 70, 5, 0, -35, 5, 5, 20, 35, 25, 5, -10, -15, 5, 0, 10, 20, 15, -20, -15, -5, 5, 0, 5, 5, 0, 0, -15, -10, -5, 5, 5, 5, -20, -30, -15, -20, -15, 0, -10, -15, -40, -40 };
int Eval::rook_mob_eg[15] = { -15, -20, -25, -25, -20, -15, -10, -5, 0, 5, 15, 15, 20, 25, 25 };
int Eval::rook_mob_mg[15] = { -20, -25, -20, -20, -15, -10, -5, 0, 10, 10, 10, 10, 20, 20, 25 };
int Eval::rook_pcsq_eg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int Eval::rook_pcsq_mg[64] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
