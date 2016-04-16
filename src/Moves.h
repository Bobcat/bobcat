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

struct MoveData {
  Move move;
  int score;
};

class MoveSorter {
public:
  virtual void sortMove(MoveData& move_data) = 0;
};

static const int LEGALMOVES = 1;
static const int STAGES = 2;
static const int QUEENPROMOTION = 4;

class Moves {
public:
  __forceinline void generateMoves(MoveSorter* sorter = 0, const Move transp_move = 0, const int flags = 0) {
    reset(sorter, transp_move, flags);
    max_stage = 3;
    if ((this->flags & STAGES) == 0) {
      generateTranspositionMove();
      generateCapturesAndPromotions();
      generateQuietMoves();
    }
  }

  __forceinline void generateCapturesAndPromotions(MoveSorter* sorter) {
    reset(sorter, 0, QUEENPROMOTION|STAGES);
    max_stage = 2;
    stage = 1;
  }

  __forceinline void generateMoves(const Piece piece, const BB& to_squares) {
    reset(0, 0, 0);
    for (BB bb = board->piece[piece]; bb; resetLSB(bb)) {
      Square from = lsb(bb);
      addMoves(piece, from, board->pieceAttacks(piece, from) & to_squares);
    }
  }

  __forceinline void generatePawnMoves(bool capture, const BB& to_squares) {
    reset(0, 0, 0);
    if (capture) {
      addPawnCaptureMoves(to_squares);
    }
    else {
      addPawnQuietMoves(to_squares);
    }
  }

  __forceinline auto nextMove() {
    while (iteration == number_moves && stage < max_stage) {
      switch (stage) {
      case 0:
        generateTranspositionMove();
        break;
      case 1:
        generateCapturesAndPromotions();
        break;
      case 2:
        generateQuietMoves();
        break;
      default: // error
        return (MoveData*)nullptr;
      }
    }
    if (iteration == number_moves) {
      return (MoveData*)nullptr;
    }
    do {
      int best_idx = iteration;
      int best_score = move_list[best_idx].score;

      for (int i = best_idx + 1; i < number_moves; i++) {
        if (move_list[i].score > best_score) {
          best_score = move_list[i].score;
          best_idx = i;
        }
      }

      if (max_stage > 2 && stage == 2 && move_list[best_idx].score < 0) {
        generateQuietMoves();
        continue;
      }
      MoveData tmp = move_list[iteration];
      move_list[iteration] = move_list[best_idx];
      move_list[best_idx] = tmp;
      return &move_list[iteration++];
    } while (1);
  }

  __forceinline auto moveCount() {
    return number_moves;
  }

  __forceinline void gotoMove(int pos) {
    iteration = pos;
  }

  __forceinline bool isPseudoLegal(const Move m) {
    // TO DO en passant moves and castle moves
    if ((bb_piece[movePiece(m)] & bbSquare(moveFrom(m))) == 0) {
      return false;
    }
    if (isCapture(m)) {
      const BB& bb_to = bbSquare(moveTo(m));
      if ((occupied_by_side[moveSide(m) ^ 1] & bb_to) == 0) {
        return false;
      }
      if ((bb_piece[moveCaptured(m)] & bb_to) == 0) {
        return false;
      }
    }
    else if (occupied & bbSquare(moveTo(m))) {
      return false;
    }
    Piece piece = movePiece(m) & 7;
    if (piece == Bishop || piece == Rook || piece == Queen) {
      if (bb_between[moveFrom(m)][moveTo(m)] & occupied) {
        return false;
      }
    }
    return true;
  }

  MoveData move_list[256];

private:
  __forceinline void reset(MoveSorter* sorter, const Move move, const int flags) {
    this->sorter = sorter;
    this->transp_move = move;
    this->flags = flags;

    if (move) {
      if (isCastleMove(this->transp_move) || isEpCapture(this->transp_move)) {
        // needed because isPseudoLegal() is not complete yet.
        this->transp_move = 0;
        this->flags &= ~STAGES;
      }
    }
    iteration = 0;
    number_moves = 0;
    stage = 0;
    if (flags & LEGALMOVES) {
      pinned = board->getPinnedPieces(side_to_move, board->king_square[side_to_move]);
    }
    occupied = board->occupied;
    occupied_by_side = board->occupied_by_side;
    bb_piece = board->piece;
  }

  __forceinline void generateTranspositionMove() {
    if (transp_move && isPseudoLegal(transp_move)) {
      move_list[number_moves].score = 890010;
      move_list[number_moves++].move = transp_move;
    }
    stage++;
  }

  void generateCapturesAndPromotions() {
    addMoves(occupied_by_side[side_to_move ^ 1]);
    const BB& pawns = board->pawns(side_to_move);
    addPawnMoves(pawnPush[side_to_move](pawns & rank_7[side_to_move]) & ~occupied, pawn_push_dist, QUIET);
    addPawnMoves(pawnWestAttacks[side_to_move](pawns) & occupied_by_side[side_to_move ^ 1], pawn_west_attack_dist, CAPTURE);
    addPawnMoves(pawnEastAttacks[side_to_move](pawns) & occupied_by_side[side_to_move ^ 1], pawn_east_attack_dist, CAPTURE);
    addPawnMoves(pawnWestAttacks[side_to_move](pawns) & en_passant_square, pawn_west_attack_dist, EPCAPTURE);
    addPawnMoves(pawnEastAttacks[side_to_move](pawns) & en_passant_square, pawn_east_attack_dist, EPCAPTURE);
    stage++;
  }

  void generateQuietMoves() {
    if (!in_check) {
      if (canCastleShort()) {
        addCastleMove(oo_king_from[side_to_move], oo_king_to[side_to_move]);
      }

      if (canCastleLong()) {
        addCastleMove(ooo_king_from[side_to_move], ooo_king_to[side_to_move]);
      }
    }
    BB pushed = pawnPush[side_to_move](board->pawns(side_to_move) & ~rank_7[side_to_move]) & ~occupied;
    addPawnMoves(pushed, pawn_push_dist, QUIET);
    addPawnMoves(pawnPush[side_to_move](pushed & rank_3[side_to_move]) & ~occupied, pawn_double_push_dist, DOUBLEPUSH);
    addMoves(~occupied);
    stage++;
  }

  __forceinline void addMove(const Piece piece, const Square from, const Square to, const Move type, const Piece promoted = 0) {
    Move move;
    Piece captured;

    if (type & CAPTURE) {
      captured = board->getPiece(to);
    }
    else if (type & EPCAPTURE) {
      captured = Pawn | ((side_to_move ^ 1) << 3);
    }
    else {
      captured = 0;
    }
    initMove(move, piece, captured, from, to, type, promoted);

    if (transp_move == move) {
      return;
    }

    if ((flags & LEGALMOVES) && !isLegal(move, piece, from, type)) {
      return;
    }
    MoveData& move_data = move_list[number_moves++];
    move_data.move = move;

    if (sorter) {
      sorter->sortMove(move_data);
    }
    else {
      move_data.score = 0;
    }
  }

  __forceinline void addMoves(const BB& to_squares) {
    BB bb;
    int offset = side_to_move << 3;
    Square from;
    for (bb = bb_piece[Queen + offset]; bb; resetLSB(bb)) {
      from = lsb(bb);
      addMoves(Queen + offset, from, queenAttacks(from, board->occupied) & to_squares);
    }
    for (bb = bb_piece[Rook + offset]; bb; resetLSB(bb)) {
      from = lsb(bb);
      addMoves(Rook + offset, from, rookAttacks(from, board->occupied) & to_squares);
    }
    for (bb = bb_piece[Bishop + offset]; bb; resetLSB(bb)) {
      from = lsb(bb);
      addMoves(Bishop + offset, from, bishopAttacks(from, board->occupied) & to_squares);
    }
    for (bb = bb_piece[Knight + offset]; bb; resetLSB(bb)) {
      from = lsb(bb);
      addMoves(Knight + offset, from, knightAttacks(from) & to_squares);
    }
    for (bb = bb_piece[King + offset]; bb; resetLSB(bb)) {
      from = lsb(bb);
      addMoves(King + offset, from, kingAttacks(from) & to_squares);
    }
  }

  __forceinline void addMoves(const Piece piece, const Square from, const BB& attacks) {
    for (BB bb = attacks; bb; resetLSB(bb)) {
      Square to = lsb(bb);
      addMove(piece | (side_to_move << 3), from, to, board->getPiece(to) == NoPiece ? QUIET : CAPTURE);
    }
  }

  __forceinline void addPawnQuietMoves(const BB& to_squares) {
    BB pushed = pawnPush[side_to_move](board->pawns(side_to_move)) & ~occupied;
    addPawnMoves(pushed & to_squares, pawn_push_dist, QUIET);
    addPawnMoves(pawnPush[side_to_move](pushed & rank_3[side_to_move]) & ~occupied & to_squares, pawn_double_push_dist, DOUBLEPUSH);
  }

  __forceinline void addPawnCaptureMoves(const BB& to_squares) {
    const BB& pawns = board->pawns(side_to_move);
    addPawnMoves(pawnWestAttacks[side_to_move](pawns) & occupied_by_side[side_to_move ^ 1] & to_squares, pawn_west_attack_dist, CAPTURE);
    addPawnMoves(pawnEastAttacks[side_to_move](pawns) & occupied_by_side[side_to_move ^ 1] & to_squares, pawn_east_attack_dist, CAPTURE);
    addPawnMoves(pawnWestAttacks[side_to_move](pawns) & en_passant_square & to_squares, pawn_west_attack_dist, EPCAPTURE);
    addPawnMoves(pawnEastAttacks[side_to_move](pawns) & en_passant_square & to_squares, pawn_east_attack_dist, EPCAPTURE);
  }

  __forceinline void addPawnMoves(const BB& to_squares, const int* dist, const Move type) {
    for (BB bb = to_squares; bb; resetLSB(bb)) {
      Square to = lsb(bb);
      Square from = to - dist[side_to_move];
      if (rankOf(to) == 0 || rankOf(to) == 7) {
        if (flags & QUEENPROMOTION) {
          addMove(Pawn | (side_to_move << 3), from, to, type | PROMOTION, Queen | (side_to_move << 3));
          return;
        }
        for (Piece promoted = Queen; promoted >= Knight; promoted--) {
          addMove(Pawn | (side_to_move << 3), from, to, type | PROMOTION, promoted | (side_to_move << 3));
        }
      }
      else {
        addMove(Pawn | (side_to_move << 3), from, to, type);
      }
    }
  }

  __forceinline void addCastleMove(const Square from, const Square to) {
    addMove(King | (side_to_move << 3), from, to, CASTLE);
  }

  __forceinline bool givesCheck(const Move m) {
    board->makeMove(m);

    if (board->isAttacked(board->king_square[side_to_move ^ 1], side_to_move)) {
      board->unmakeMove(m);
      return true;
    }
    board->unmakeMove(m);
    return false;
  }

  __forceinline bool isLegal(const Move m, const Piece piece, const Square from, Move type) {
    if ((pinned & bbSquare(from)) || in_check || (piece & 7) == King || (type & EPCAPTURE)) {
      board->makeMove(m);
      if (board->isAttacked(board->king_square[side_to_move], side_to_move ^ 1)) {
        board->unmakeMove(m);
        return false;
      }
      board->unmakeMove(m);
    }
    return true;
  }

  __forceinline bool canCastleShort() {
    return (castle_rights & oo_allowed_mask[side_to_move])
           && isCastleAllowed(oo_king_to[side_to_move], side_to_move);
  }

  __forceinline bool canCastleLong() {
    return (castle_rights & ooo_allowed_mask[side_to_move])
           && isCastleAllowed(ooo_king_to[side_to_move], side_to_move);
  }

  __forceinline bool isCastleAllowed(Square to, const Side side_to_move) {
    // A bit complicated because of Chess960. See http://en.wikipedia.org/wiki/Chess960
    // The following comments were taken from that source.

    // Check that the smallest back rank interval containing the king, the castling rook, and their
    // destination squares, contains no pieces other than the king and castling rook.

    Square rook_to = rook_castles_to[to];
    Square rook_from = rook_castles_from[to];
    Square king_square = board->king_square[side_to_move];

    BB bb_castle_pieces = bbSquare(rook_from) | bbSquare(king_square);

    BB bb_castle_span = bb_castle_pieces
                        | bbSquare(rook_to) | bbSquare(to)
                        | bb_between[king_square][rook_from]
                        | bb_between[rook_from][rook_to];

    if ((bb_castle_span & occupied) != bb_castle_pieces) {
      return false;
    }

    // Check that no square between the king's initial and final squares (including the initial and final
    // squares) may be under attack by an enemy piece. (Initial square was already checked a this point.)

    for (BB bb = bb_between[king_square][to] | bbSquare(to); bb; resetLSB(bb)) {
      if (board->isAttacked(lsb(bb), side_to_move ^ 1)) {
        return false;
      }
    }
    return true;
  }

public:
  Side side_to_move;
  int castle_rights;
  bool in_check;
  BB en_passant_square;
  Board* board;

private:
  BB* bb_piece;
  BB* occupied_by_side;
  BB occupied;
  int iteration;
  int stage;
  int max_stage;
  int number_moves;
  BB pinned;
  MoveSorter* sorter;
  Move transp_move;
  int flags;
};
