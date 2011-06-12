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

class Material {
public:
	int max_value;
	
	Material() {
		max_value = 2*(2*piece_value[Knight]+2*piece_value[Bishop]+2*piece_value[Rook]+piece_value[Queen]);
	}

	__forceinline void clear() {
		key[0] = key[1] = 0;
		material_value[0] = material_value[1] = 0;
	}

	__forceinline void remove(const Piece p) {
		updateKey(p >> 3, p & 7, -1);
		material_value[p >> 3] -= Material::piece_value[p & 7];
	}

	__forceinline void add(const Piece p) {
		updateKey(p >> 3, p & 7, 1);
		material_value[p >> 3] += Material::piece_value[p & 7];
	}

	__forceinline void updateKey(const int c, const int p, const int delta) {
		if (p != King) {
			int x = count(c, p) + delta;
			key[c] &= ~(15 << bit_shift[p]);
			key[c] |= (x << bit_shift[p]); 
		}
	}
	
	__forceinline int count(const Side c, const Piece p) {
		return (key[c] >> bit_shift[p]) & 15;
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

	__forceinline bool hasPawnsOnly(const Side c) {
		return key[c] == (key[c] & 15); 
	}

	__forceinline int value() {
		return material_value[0] + material_value[1];
	}

	__forceinline int balance() {
		return material_value[0] - material_value[1];
	}

	__forceinline int pawnValue() {
		return (key[0] & 15) * Material::piece_value[Pawn] + (key[1] & 15) * Material::piece_value[Pawn];
	}

	__forceinline int pawnCount() {
		return (key[0] & 15) + (key[1] & 15);
	}

	__forceinline int pawnCount(int side) {
		return key[side] & 15;
	}

	__forceinline int evaluate(bool& recognized_score, int eval, int side, const Board* board) {
		uint32 key1, key2;
		bool flip;
		int score;
		if (key[side] >= key[side ^ 1]) {
			key1 = key[side];
			key2 = key[side ^ 1];
			score = eval;
			flip = false;
		}
		else {
			key2 = key[side];
			key1 = key[side ^ 1];
			score = -eval;
			flip = true;
		}
		this->board = board;
		recognized_draw = false;
		if ((key1 & ~all_pawns) == key1) {
			switch (key1) {
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
					score = KBNKX(score, key2, flip ? side ^ 1 : side);
					break;
				case kb:
					score = KBKX(score, key2);
					break;
				case knn:
					score = KNNKX(score, key2);
					break;
				case kn:
				case k:
					score = key2 == k ? 0 : min(0, score);
					break;
				default:
					break;
			}
		}
		else {
			switch (key1 & ~all_pawns) {
				case kb:
					score = KBxKX(score, key1, key2, flip ? side ^ 1 : side);
					break;
				case k:
					score = KxKx(score, key1, key2, flip ? side ^ 1 : side);
					break;
				default:
					break;
			}
		}
		recognized_score = recognized_draw;
		return flip ? -score : score;
	}

	__forceinline int KRBKX(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case kr:
				if (key2 & all_pawns) return min(eval/8, eval); 
				else return eval/8;
			case kbb:
			case kbn:
			case knn:
				if (key2 & all_pawns) return min(eval/4, eval); 
				else return eval/4;
			default:
				break;
		}
		return eval;
	}

	__forceinline int KRNKX(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case kr:
				if (key2 & all_pawns) return min(eval/16, eval); 
				else return eval/16;
			case kbb:
			case kbn:
			case knn:
				if (key2 & all_pawns) return min(eval/8, eval); 
				else return eval/8;
			default:
				break;
		}
		return eval;
	}

	__forceinline int KRKX(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case kbb:
			case kbn:
			case knn:
				if ((key2 & all_pawns) == 0) return eval/8; 
				break;
			case kb:
			case kn:
				if (key2 & all_pawns) return min(eval/4, eval); 
				return eval/4;
			default:
				break;
		}
		return eval;
	}

	__forceinline int KBBKX(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case kb:
				if (key2 & all_pawns) return min(eval/8, eval); 
				else return eval/8;
			case kn:
				break;
			default:
				break;
		}
		return eval;
	}

	__forceinline int KBNKX(int eval, uint32 key2, int side1) {
		switch (key2 & ~all_pawns) {
			case k:
				if ((key2 & all_pawns) == 0) return KBNK(eval, side1);
				break;
			case kb:
				if (key2 & all_pawns) return min(eval/8, eval); 
				else return eval/8;
			case kn:
				if (key2 & all_pawns) return min(eval/4, eval); 
				else return eval/4;
			default:
				break;
		}
		return eval;
	}

	__forceinline int KBNK(int eval, int side1) {
		int loosing_king_square = board->king_square[side1 ^ 1];
		int winning_corner1 = a8, winning_corner2 = h1;
		if (isDark(lsb(board->bishops(side1)))) {
			winning_corner1 = a1;
			winning_corner2 = h8;
		}
		return eval + 175 - min(25*chebyshev_distance[winning_corner1][loosing_king_square], 
			25*chebyshev_distance[winning_corner2][loosing_king_square]);
	}

	__forceinline int KBKX(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case knn:
				if ((key2 & all_pawns) == 0) return eval/8; 
				break;
			default:
				break;
		}
		return min(0, eval);
	}

	__forceinline int KNNKX(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case k:
			case kn:
				if (key2 & all_pawns) return min(0, eval); 
				else return recognizedDraw();
			default:
				break;
		}
		return eval;
	}

	__forceinline int KBxKX(int eval, uint32 key1, uint32 key2, int side1) {
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

	__forceinline int KBxKx(int eval, uint32 key1, uint32 key2, int side1) {
		if ((key1 & all_pawns) == 1 && (key2 & all_pawns) == 0) {
			return KBpK(eval, side1);
		}
		return eval;
	}
	// fen 7k/8/8/8/4B3/2K4P/8/8 w - - 5 4 
	// fen 7k/7P/8/8/4B3/2K5/8/8 w - - 0 1
	// fen 7Q/6k1/8/8/4B3/2K5/8/8 b - - 0 1
	__forceinline int KBpK(int eval, int side1) {
		Square pawnsq1 = lsb(board->pawns(side1));
		Square promosq1 = side1 == 1 ? file(pawnsq1) : file(pawnsq1) + 56;
		if (!sameColor(promosq1, lsb(board->bishops(side1)))) {
			int side2 = side1 ^ 1;
			Square ksq2 = board->king_square[side2];
			if (
				(promosq1 == h8 && (ksq2 == h8 || ksq2 == h7 || ksq2 == g8 || ksq2 == g7)) ||
				(promosq1 == a8 && (ksq2 == a8 || ksq2 == a7 || ksq2 == b8 || ksq2 == b7)) ||
				(promosq1 == h1 && (ksq2 == h1 || ksq2 == h2 || ksq2 == g1 || ksq2 == g2)) ||
				(promosq1 == a1 && (ksq2 == a1 || ksq2 == a2 || ksq2 == b1 || ksq2 == b2)))
			{
				return recognizedDraw();
			}
		}
		return eval;
	}

	__forceinline int KxKx(int eval, uint32 key1, uint32 key2, int side1) {
		if ((key1 & all_pawns) == 1 && (key2 & all_pawns) == 0) {
			return KpK(eval, side1);
		}
		return eval;
	}

	__forceinline int KpK(int eval, int side1) {
		Square pawnsq1 = lsb(board->pawns(side1));
		Square promosq1 = side1 == 1 ? file(pawnsq1) : file(pawnsq1) + 56;
		int side2 = side1 ^ 1;
		Square ksq2 = board->king_square[side2];
		if (
			(promosq1 == h8 && (ksq2 == h8 || ksq2 == h7 || ksq2 == g8 || ksq2 == g7)) ||
			(promosq1 == a8 && (ksq2 == a8 || ksq2 == a7 || ksq2 == b8 || ksq2 == b7)) ||
			(promosq1 == h1 && (ksq2 == h1 || ksq2 == h2 || ksq2 == g1 || ksq2 == g2)) ||
			(promosq1 == a1 && (ksq2 == a1 || ksq2 == a2 || ksq2 == b1 || ksq2 == b2)))
		{
			return recognizedDraw();
		}
		return eval;
	}

	__forceinline int recognizedDraw() {
		recognized_draw = true;
		return 0;
	}

	bool recognized_draw;
	uint32 key[2];
	int material_value[2];
	const Board* board;

	static int bit_shift[7];
	static int piece_value[6];
	
	static const uint32 k   = 0x00000;
	static const uint32 kn  = 0x00010;
	static const uint32 kb  = 0x00100;
	static const uint32 kr  = 0x01000;
	static const uint32 krr = 0x02000;
	static const uint32 kbb = 0x00200;
	static const uint32 kbn = 0x00110;
	static const uint32 knn = 0x00020;
	static const uint32 krn = 0x01010;
	static const uint32 krb = 0x01100;

	static const uint32 all_pawns = 0xf;
};

int Material::bit_shift[7] = {0, 4, 8, 12, 16, 20};
int Material::piece_value[6] = { 100, 400, 400, 600, 1200, 0 };

#define piece_value(p) (Material::piece_value[p & 7])
//KRBKR,KRBKBB,KRBKBN,KRBKNN
//KRNKR,KRNKBB,KRNKBN,KRNKNN
//KRKBB,KRKBN,KRKB,KRKNN,KRKN
//KBBKB,KBBKN
//KBNK,KBNKB,KBNKN
//KBKNN
//KNNK,KNNKN
//
