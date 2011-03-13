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

class Board {
public:
	BB piece[2 << 3], occupied_by_side[2], occupied;
	int board[64];
	Square king_square[2];

	void clear() {
		memset(piece, 0, sizeof(piece));
		memset(occupied_by_side, 0, sizeof(occupied_by_side));
		for (int sq = 0; sq < 64; sq++) board[sq] = NoPiece;
		king_square[0] = king_square[1] = -1;
		occupied = 0;
	}
	
	void addPiece(const Piece p, const Side side, const Square sq) {
		piece[p + (side << 3)] |= bb_square(sq);
		occupied_by_side[side] |= bb_square(sq);
		occupied |= bb_square(sq);
		board[sq] = p + (side << 3);
		if (p == King) {
			king_square[side] = sq;
		}
	}

	__forceinline void removePiece(const int p, const int sq) {
		piece[p] &= ~bb_square(sq);
		occupied_by_side[p >> 3] &= ~bb_square(sq);
		occupied &= ~bb_square(sq);
		board[sq] = NoPiece;
	}

	__forceinline void addPiece(const int p, const int sq) {
		piece[p] |= bb_square(sq);
		occupied_by_side[p >> 3] |= bb_square(sq);
		occupied |= bb_square(sq);
		board[sq] = p;
	}

	//__forceinline 
	void makeMove(const Move m) {
		removePiece(PIECE(m), FROM(m));
		if (isEpCapture(m)) {
			if (PIECE(m) < 8) {
				removePiece(CAPTURED(m), TO(m) - 8);
			}
			else {
				removePiece(CAPTURED(m), TO(m) + 8);
			}
		}
		else if (isCapture(m)) {
			removePiece(CAPTURED(m), TO(m));
		}
		if (TYPE(m) & PROMOTION) {
			addPiece(PROMOTED(m), TO(m));
		}
		else {
			addPiece(PIECE(m), TO(m));
		}
		if (isCastleMove(m)) {
			removePiece(Rook + sideMask(m), rook_castles_from[TO(m)]);
			addPiece(Rook + sideMask(m), rook_castles_to[TO(m)]);
		}
		if ((PIECE(m) & 7) == King) {
			king_square[side(m)] = TO(m);
		}
	}

	//__forceinline
	void unmakeMove(const Move m) {
		if (TYPE(m) & PROMOTION) {
			removePiece(PROMOTED(m), TO(m));
		}
		else {
			removePiece(PIECE(m), TO(m));
		}
		if (isEpCapture(m)) {
			if (PIECE(m) < 8) {
				addPiece(CAPTURED(m), TO(m) - 8);
			}
			else {
				addPiece(CAPTURED(m), TO(m) + 8);
			}
		}
		else if (isCapture(m)) {
			addPiece(CAPTURED(m), TO(m));
		}
		addPiece(PIECE(m), FROM(m));
		if (isCastleMove(m)) {
			removePiece(Rook + sideMask(m), rook_castles_to[TO(m)]);
			addPiece(Rook + sideMask(m), rook_castles_from[TO(m)]);
		}
		if ((PIECE(m) & 7) == King) {
			king_square[side(m)] = FROM(m);
		}
	}

	__forceinline Piece getPiece(const Square sq) const {
		return board[sq];
	}
	
	__forceinline const BB getPinnedPieces(const Side side, const Square sq) {
		BB pinned_pieces = 0; 
		Side opp = side ^ 1; 
		BB pinners = xrayBishopAttacks(occupied, occupied_by_side[side], sq) & 
			(piece[Bishop + ((opp) << 3)] | piece[Queen | ((opp) << 3)]);

		while (pinners) {
			pinned_pieces |= bb_between[lsb(pinners)][sq] & occupied_by_side[side];
			reset_lsb(pinners);
		}
		pinners = xrayRookAttacks(occupied, occupied_by_side[side], sq) & 
			(piece[Rook + (opp << 3)] | piece[Queen | (opp << 3)]);

		while (pinners) {
			pinned_pieces |= bb_between[lsb(pinners)][sq] & occupied_by_side[side];
			reset_lsb(pinners);
		}
		return pinned_pieces;
	}

	__forceinline BB xrayRookAttacks(const BB& occupied, BB blockers, const Square sq) {
		BB attacks = Rmagic(sq, occupied);
		blockers &= attacks;
		return attacks ^ Rmagic(sq, occupied ^ blockers);
	}

	__forceinline BB xrayBishopAttacks(const BB& occupied, BB blockers, const Square sq) {
		BB attacks = Bmagic(sq, occupied);
		blockers &= attacks;
		return attacks ^ Bmagic(sq, occupied ^ blockers);
	}

	__forceinline bool isPawnPassed(const Square sq, const Side side) {
		return (passed_pawn_front_spans[side][sq] & piece[Pawn + ((side ^ 1) << 3)]) == 0;
	}

	__forceinline bool isPieceOnSquare(const Piece p, const Square sq, const Side side) {
		return ((bb_square(sq) & piece[p + (side << 3)]) != 0);
	}

	__forceinline bool isPieceOnFile(const Piece p, const Square sq, const Side side) {
		return ((bb_file(sq) & piece[p + (side << 3)]) != 0);
	}

	__forceinline bool isPawnIsolated(const Square sq, const Side side) {
		return (piece[Pawn + (side << 3)] & neighbourFiles(bb_square(sq))) == 0;
	}

	__forceinline bool isPawnBehind(const Square sq, const Side side) { 
		BB bb = bb_square(sq);
		return (piece[Pawn + (side << 3)] & (pawnFill[side ^ 1](westOne(bb) | eastOne(bb)))) == 0;
	}

	#define isOccupied(x) (bb_square(x) & occupied)

	__forceinline bool isCastleAllowed(Square to) {
		switch (to) {
			case g1:
				if (isOccupied(f1) || isOccupied(g1) || isAttacked(f1, 1) || isAttacked(g1, 1)) {
					return false;
				}
				break;
			case c1:
				if (isOccupied(b1) || isOccupied(c1) || isOccupied(d1) || isAttacked(c1, 1) || isAttacked(d1, 1)) {
					return false;
				}
				break;
			case g8:
				if (isOccupied(f8) || isOccupied(g8) || isAttacked(f8, 0) || isAttacked(g8, 0)) {
					return false;
				}
				break;
			case c8:
				if (isOccupied(b8) || isOccupied(c8) || isOccupied(d8) || isAttacked(c8, 0) || isAttacked(d8, 0)) {
					return false;
				}
				break;
			default: //error
				break;
		}
		return true;
	}

	__forceinline bool isAttacked(const Square sq, const Side side) {
		return	isAttackedBySlider(sq, side) || isAttackedByKnight(sq, side) || isAttackedByPawn(sq, side) 
			|| isAttackedByKing(sq, side);
	}

	__forceinline BB knightAttacks(const Square sq) { 
		return knight_attacks[sq]; 
	}

	__forceinline BB bishopAttacks(const Square sq) { 
		return Bmagic(sq, occupied);
	}

	__forceinline BB rookAttacks(const Square sq) { 
		return Rmagic(sq, occupied);
	}

	__forceinline BB queenAttacks(const Square sq) { 
		return Qmagic(sq, occupied);
	}

	__forceinline BB kingAttacks(const Square sq) { 
		return king_attacks[sq]; 
	}

	__forceinline bool isAttackedBySlider(const Square sq, const Side side) {
		BB r_attacks = rookAttacks(sq);
		if (piece[Rook + (side << 3)] & r_attacks) {
			return true;
		}
		BB b_attacks = bishopAttacks(sq);
		if (piece[Bishop + (side << 3)] & b_attacks) {
			return true;
		}
		else if (piece[Queen + (side << 3)] & (b_attacks | r_attacks)) {
			return true;
		}
		return false;
	}

	__forceinline bool isAttackedByKnight(const Square sq, const Side side) {
		return (piece[Knight + (side << 3)] & knight_attacks[sq]) != 0;
	}

	__forceinline bool isAttackedByPawn(const Square sq, const Side side) {
		return (piece[Pawn | (side << 3)] & pawn_captures[sq | ((side ^ 1) << 6)]) != 0;
	}

	__forceinline bool isAttackedByKing(const Square sq, const Side side) {
		return (piece[King | (side << 3)] & king_attacks[sq]) != 0;
	}

	void print_board(ostream& os) const {
		static char piece_letter[] = "PNBRQK. pnbrqk. "; 
		os << endl;
		for (int rank = 7; rank >= 0; rank--) {
			os << rank + 1 << "  ";
			for (int file = 0; file <= 7; file++) {
				Piece p_and_c = getPiece(rank * 8 + file);
				os << piece_letter[p_and_c] << " ";
			}
			os << endl;
		}
		os << endl << "   a b c d e f g h" << endl;
	}

	__forceinline const BB& pawns(int side) const { return piece[Pawn | (side << 3)]; }
	__forceinline const BB& knights(int side) const { return piece[Knight | (side << 3)]; }
	__forceinline const BB& bishops(int side) const { return piece[Bishop | (side << 3)]; }
	__forceinline const BB& rooks(int side) const { return piece[Rook | (side << 3)]; }
	__forceinline const BB& queens(int side) const { return piece[Queen | (side << 3)]; }
	__forceinline const BB& king(int side) const { return piece[King | (side << 3)]; }
};
