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

class See {
public:
  See(Game* game) : board_(game->board) {
  }

  int seeMove(const Move move) {
    int score;
    board_.makeMove(move);
    if (!board_.isAttacked(board_.king_square[moveSide(move)], moveSide(move) ^ 1)) {
      initialiseSeeMove();
      score = seeRec(materialChange(move), nextToCapture(move), moveTo(move), moveSide(move) ^ 1);
    }
    else {
      score = SEE_INVALID_SCORE;
    }
    board_.unmakeMove(move);
    return score;
  }

  int seeLastMove(const Move move) {
    initialiseSeeMove();
    return seeRec(materialChange(move), nextToCapture(move), moveTo(move), moveSide(move) ^ 1);
  }

private:
  __forceinline int materialChange(const Move move) {
    return (isCapture(move) ? piece_value(moveCaptured(move)) : 0) + (isPromotion(move) ?
           (piece_value(movePromoted(move)) - piece_value(Pawn)) : 0);
  }

  __forceinline int nextToCapture(const Move move) {
    return isPromotion(move) ? movePromoted(move) : movePiece(move);
  }

  int seeRec(const int material_change, const Piece next_to_capture, const Square to, const Side side_to_move) {
    Square from;
    Move move;

    do {
      if (!lookupBestAttacker(to, side_to_move, from)) {
        return material_change;
      }
      if ((current_piece[side_to_move] == Pawn) && (rankOf(to) == 0 || rankOf(to) == 7)) {
        initMove(move, current_piece[side_to_move] | (side_to_move << 3), next_to_capture,
                 from, to, PROMOTION | CAPTURE, Queen | (side_to_move << 3));
      }
      else {
        initMove(move, current_piece[side_to_move] | (side_to_move << 3), next_to_capture,
                 from, to, CAPTURE, 0);
      }
      board_.makeMove(move);
      if (!board_.isAttacked(board_.king_square[side_to_move], side_to_move ^ 1)) {
        break;
      }
      board_.unmakeMove(move);
    }
    while (1);

    int score = -seeRec(materialChange(move), nextToCapture(move), moveTo(move), moveSide(move) ^ 1);

    board_.unmakeMove(move);

    return (score < 0) ? material_change + score : material_change;
  }

  __forceinline bool lookupBestAttacker(const Square to, const Side side, Square& from) { // "Best" == "Lowest piece value"
    switch (current_piece[side]) {
    case Pawn:
      if (current_piece_bitboard[side] & pawn_captures[to | ((side ^ 1) << 6)]) {
        from = lsb(current_piece_bitboard[side] & pawn_captures[to | ((side ^ 1) << 6)]);
        current_piece_bitboard[side] &= ~bbSquare(from);
        return true;
      }
      current_piece[side]++;
      current_piece_bitboard[side] = board_.knights(side);
    case Knight:
      if (current_piece_bitboard[side] & knightAttacks(to)) {
        from = lsb(current_piece_bitboard[side] & knightAttacks(to));
        current_piece_bitboard[side] &= ~bbSquare(from);
        return true;
      }
      current_piece[side]++;
      current_piece_bitboard[side] = board_.bishops(side);
    case Bishop:
      if (current_piece_bitboard[side] & bishopAttacks(to, board_.occupied)) {
        from = lsb(current_piece_bitboard[side] & bishopAttacks(to, board_.occupied));
        current_piece_bitboard[side] &= ~bbSquare(from);
        return true;
      }
      current_piece[side]++;
      current_piece_bitboard[side] = board_.rooks(side);
    case Rook:
      if (current_piece_bitboard[side] & rookAttacks(to, board_.occupied)) {
        from = lsb(current_piece_bitboard[side] & rookAttacks(to, board_.occupied));
        current_piece_bitboard[side] &= ~bbSquare(from);
        return true;
      }
      current_piece[side]++;
      current_piece_bitboard[side] = board_.queens(side);
    case Queen:
      if (current_piece_bitboard[side] & queenAttacks(to, board_.occupied)) {
        from = lsb(current_piece_bitboard[side] & queenAttacks(to, board_.occupied));
        current_piece_bitboard[side] &= ~bbSquare(from);
        return true;
      }
      current_piece[side]++;
      current_piece_bitboard[side] = board_.king(side);
    case King:
      if (current_piece_bitboard[side] & kingAttacks(to)) {
        from = lsb(current_piece_bitboard[side] & kingAttacks(to));
        current_piece_bitboard[side] &= ~bbSquare(from);
        return true;
      }
    default:
      return false;
    }
  }

protected:
  __forceinline void initialiseSeeMove() {
    current_piece[0] = Pawn;
    current_piece_bitboard[0] = board_.piece[Pawn];
    current_piece[1] = Pawn;
    current_piece_bitboard[1] = board_.piece[Pawn | 8];
  }

  BB current_piece_bitboard[2];
  int current_piece[2];
  Board& board_;

  static const int SEE_INVALID_SCORE = -5000;
};
