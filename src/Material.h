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

static const int RECOGNIZEDDRAW = 1;

class Material {
public:
  Material() {
    max_value_without_pawns = 2*(2*piece_value[Knight]+2*piece_value[Bishop]+
                                 2*piece_value[Rook]+piece_value[Queen]);

    max_value = max_value_without_pawns + 2*8*piece_value[Pawn];
  }

  __forceinline void clear() {
    key[0] = key[1] = 0;
    material_value[0] = material_value[1] = 0;
  }

  __forceinline void remove(const Piece p) {
    updateKey(p >> 3, p & 7, -1);
    material_value[p >> 3] -= piece_value[p & 7];
  }

  __forceinline void add(const Piece p) {
    updateKey(p >> 3, p & 7, 1);
    material_value[p >> 3] += piece_value[p & 7];
  }

  __forceinline void updateKey(const int c, const int p, const int delta) {
    if (p != King) {
      int x = count(c, p) + delta;
      key[c] &= ~(15 << piece_bit_shift[p]);
      key[c] |= (x << piece_bit_shift[p]);
    }
  }

  __forceinline int count(const Side c, const Piece p) {
    return (key[c] >> piece_bit_shift[p]) & 15;
  }

  __forceinline void makeMove(const Move m) {
    if (isCapture(m)) {
      remove(moveCaptured(m));
    }
    if (moveType(m) & PROMOTION) {
      remove(movePiece(m));
      add(movePromoted(m));
    }
  }

  __forceinline bool isKx(const Side c) {
    return key[c] == (key[c] & 15);
  }

  __forceinline bool isKx() {
    return isKx(0) && isKx(1);
  }

  __forceinline int value() {
    return material_value[0] + material_value[1];
  }

  __forceinline int value(const Side c) {
    return material_value[c];
  }

  __forceinline int balance() {
    return material_value[0] - material_value[1];
  }

  __forceinline int pawnValue() {
    return (key[0] & 15) * piece_value[Pawn] + (key[1] & 15) * piece_value[Pawn];
  }

  __forceinline int pawnCount() {
    return (key[0] & 15) + (key[1] & 15);
  }

  __forceinline int pawnCount(int side) {
    return key[side] & 15;
  }

  __forceinline int evaluate(int& flags, int eval, int side_to_move, const Board* board) {
    this->flags = 0;
    uint32_t key1;
    uint32_t key2;
    int score;
    Side side1;

    if (key[side_to_move] >= key[side_to_move ^ 1]) {
      key1 = key[side_to_move];
      key2 = key[side_to_move ^ 1];
      side1 = side_to_move;
      score = eval;
    }
    else {
      key1 = key[side_to_move ^ 1];
      key2 = key[side_to_move];
      side1 = side_to_move ^ 1;
      score = -eval;
    }
    Side side2 = side1 ^ 1;
    int pc1 = pawnCount(side1);
    int pc2 = pawnCount(side2);

    this->board = board;
    drawish = 0;

    switch (key1 & ~all_pawns) {
    case kqb:
      score = KQBKX(score, key2);
      break;
    case kqn:
      score = KQNKX(score, key2);
      break;
    case krb:
      score = KRBKX(score, key2);
      break;
    case krn:
      score = KRNKX(score, key2);
      break;
    case kr:
      score = KRKX(score, key2);
      break;
    case kbb:
      score = KBBKX(score, key2);
      break;
    case kbn:
      score = KBNKX(score, key2, pc1, pc2, side1);
      break;
    case kb:
      score = KBKX(score, key1, key2, pc1, pc2, side1, side2, side_to_move);
      break;
    case kn:
      score = KNKX(score, key2, pc1, pc2, side1, side2, side_to_move);
      break;
    case knn:
      score = KNNKX(score, key2, pc1);
      break;
    case k:
      score = KKx(score, key1, key2, pc1, pc2, side1);
      break;
    default:
      break;
    }
    if (drawish) {
      int drawish_score = score/drawish;
      if (pc1 + pc2 == 0) {
        score = drawish_score;
      }
      else if (pc1 == 0) {
        score = std::min(drawish_score, score);
      }
      else if (pc2 == 0) {
        score = std::max(drawish_score, score);
      }
    }
    flags = this->flags;
    return side1 != side_to_move ? -score : score;
  }

  __forceinline int KQBKX(int eval, uint32_t key2) {
    switch (key2 & ~all_pawns) {
    case kq:
      drawish = 16;
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KQNKX(int eval, uint32_t key2) {
    switch (key2 & ~all_pawns) {
    case kq:
      drawish = 16;
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KRBKX(int eval, uint32_t key2) {
    switch (key2 & ~all_pawns) {
    case kr:
      drawish = 16;
      break;
    case kbb:
    case kbn:
    case knn:
      drawish = 8;
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KRNKX(int eval, uint32_t key2) {
    switch (key2 & ~all_pawns) {
    case kr:
      drawish = 32;
      break;
    case kbb:
    case kbn:
    case knn:
      drawish = 16;
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KRKX(int eval, uint32_t key2) {
    switch (key2 & ~all_pawns) {
    case kbb:
    case kbn:
    case knn:
      drawish = 16;
      break;
    case kb:
    case kn:
      drawish = 8;
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KBBKX(int eval, uint32_t key2) {
    switch (key2 & ~all_pawns) {
    case kb:
      drawish = 16;
      break;
    case kn:
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KBNKX(int eval, uint32_t key2, int pc1, int pc2, int side1) {
    switch (key2 & ~all_pawns) {
    case k:
      if (pc1 + pc2 == 0) {
        return KBNK(eval, side1);
      }
      break;
    case kb:
      drawish = 8;
      break;
    case kn:
      drawish = 4;
      break;
    default:
      break;
    }
    return eval;
  }

  __forceinline int KBNK(int eval, int side1) {
    int loosing_kingsq = board->king_square[side1 ^ 1];
    int a_winning_cornersq = a8;
    int another_winning_cornersq = h1;
    if (isDark(lsb(board->bishops(side1)))) {
      a_winning_cornersq = a1;
      another_winning_cornersq = h8;
    }
    return eval + 175 - std::min(25*distance[a_winning_cornersq][loosing_kingsq],
                                 25*distance[another_winning_cornersq][loosing_kingsq]);
  }

  __forceinline int KBKX(int eval, uint32_t key1, uint32_t key2, int pc1, int pc2, const Side side1, const Side side2,
                         int side_to_move)
  {
    if (pc1 > 0) {
      return KBxKX(eval, key1, key2, side1);
    }
    switch (key2 & ~all_pawns) {
    case k: {
      if (pc1 + pc2 == 0) {
        return drawScore();
      }
      else if (pc1 == 0 && pc2 == 1) {
        const BB& bishopbb = board->bishops(side1);
        if (side1 == side_to_move || !board->isAttacked(lsb(board->bishops(side1)), side2)) {
          if (pawn_front_span[side2][lsb(board->pawns(side2))] &
              (board->bishopAttacks(lsb(bishopbb)) | bishopbb))
          {
            return drawScore();
          }
        }
      }
      break;
    }
    case kb:
    case knn:
    case kn:
      drawish = 16;
      break;
    default:
      break;
    }
    return std::min(0, eval);
  }

  __forceinline int KNKX(int eval, uint32_t key2, int pc1, int pc2, const Side side1, const Side side2, int side_to_move) {
    switch (key2 & ~all_pawns) {
    case k: {
      if (pc1 + pc2 == 0) {
        return drawScore();
      }
      if (pc1 == 0 && pc2 == 1) {
        const BB& knightbb = board->knights(side1);
        if (side1 == side_to_move || !board->isAttacked(lsb(board->knights(side1)), side2)) {
          if (pawn_front_span[side2][lsb(board->pawns(side2))] &
              (board->knightAttacks(lsb(knightbb)) | knightbb))
          {
            return drawScore();
          }
        }
      }
      break;
    }
    case kn:
      drawish = 16;
      break;
    default:
      break;
    }
    return pc1 == 0 ? std::min(0, eval) : eval;
  }

  __forceinline int KNNKX(int eval, uint32_t key2, int pc1) {
    switch (key2 & ~all_pawns) {
    case k:
    case kn:
      drawish = 32;
      break;
    default:
      break;
    }
    return pc1 == 0 ? std::min(0, eval) : eval;
  }

  __forceinline int KKx(int eval, uint32_t key1, uint32_t key2, int pc1, int pc2, int side1) {
    if (pc1 + pc2 == 0) {
      return drawScore();
    }
    else if (pc2 > 0) {
      return KxKx(eval, key1, key2, pc1, pc2, side1);
    }
    return eval;
  }

  __forceinline int KBxKX(int eval, uint32_t key1, uint32_t key2, int side1) {
    switch (key2 & ~all_pawns) {
    case kb:
      if (!sameColor(lsb(board->bishops(0)), lsb(board->bishops(1))) &&
          abs(pawnCount(0) - pawnCount(1)) <= 2)
      {
        return eval/2;
      }
      break;
    case k:
      return KBxKx(eval, key1, key2, side1);
    default:
      break;
    }
    return eval;
  }

  __forceinline int KBxKx(int eval, uint32_t key1, uint32_t key2, int side1) {
    if ((key1 & all_pawns) == 1 && (key2 & all_pawns) == 0) {
      return KBpK(eval, side1);
    }
    return eval;
  }

  // fen 8/6k1/8/8/3K4/5B1P/8/8 w - - 0 1
  __forceinline int KBpK(int eval, int side1) {
    Square pawnsq1 = lsb(board->pawns(side1));
    Square promosq1 = side1 == 1 ? file(pawnsq1) : file(pawnsq1) + 56;
    if (!sameColor(promosq1, lsb(board->bishops(side1)))) {
      const BB& bbk2 = board->king(side1 ^ 1);
      if ((promosq1 == h8 && (bbk2 & corner_h8)) ||
          (promosq1 == a8 && (bbk2 & corner_a8)) ||
          (promosq1 == h1 && (bbk2 & corner_h1)) ||
          (promosq1 == a1 && (bbk2 & corner_a1)))
      {
        return drawScore();
      }
    }
    return eval;
  }

  __forceinline int KxKx(int eval, uint32_t key1, uint32_t key2, int pc1, int pc2, int side1) {
    if (pc1 == 1 && pc2 == 0) {
      return KpK(eval, side1);
    }
    return eval;
  }

  __forceinline int KpK(int eval, int side1) {
    Square pawnsq1 = lsb(board->pawns(side1));
    Square promosq1 = side1 == 1 ? file(pawnsq1) : file(pawnsq1) + 56;
    const BB& bbk2 = board->king(side1 ^ 1);
    if ((promosq1 == h8 && (bbk2 & corner_h8)) ||
        (promosq1 == a8 && (bbk2 & corner_a8)) ||
        (promosq1 == h1 && (bbk2 & corner_h1)) ||
        (promosq1 == a1 && (bbk2 & corner_a1)))
    {
      return drawScore();
    }
    return eval;
  }

  __forceinline int drawScore() {
    flags |= RECOGNIZEDDRAW;
    return 0;
  }

  int drawish;
  int flags;
  uint32_t key[2];
  int material_value[2];
  const Board* board;
  int max_value_without_pawns;
  int max_value;


  static int piece_bit_shift[7];
  static int piece_value[6];

  static const uint32_t k   = 0x00000;
  static const uint32_t kp  = 0x00001;
  static const uint32_t kn  = 0x00010;
  static const uint32_t kb  = 0x00100;
  static const uint32_t kr  = 0x01000;
  static const uint32_t kq  = 0x10000;
  static const uint32_t krr = 0x02000;
  static const uint32_t kbb = 0x00200;
  static const uint32_t kbn = 0x00110;
  static const uint32_t knn = 0x00020;
  static const uint32_t krn = 0x01010;
  static const uint32_t krb = 0x01100;
  static const uint32_t kqb = 0x10100;
  static const uint32_t kqn = 0x10010;

  static const uint32_t all_pawns = 0xf;
};

int Material::piece_bit_shift[7] = {0, 4, 8, 12, 16, 20};
int Material::piece_value[6] = { 100, 400, 400, 600, 1200, 0 };

#define piece_value(p) (Material::piece_value[p & 7])
