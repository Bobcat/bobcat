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

	void remove(const Piece p) {
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
			remove(CAPTURED(m));
		}
		if (TYPE(m) & PROMOTION) {
			remove(PIECE(m));
			add(PROMOTED(m));
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

	__forceinline int evaluate(int eval, int side, const Board* board) {
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
		/*
		kq kbb /4
		*/
		this->board = board;
		switch (key1) {
 			case krn:
				score = KRNKx(score, key2);
				break;
			case krb:
				score = KRBKx(score, key2);
				break;
			case kr:
				score = KRKx(score, key2);
				break;
			case kbb:
				score = KBBKx(score, key2);
				break;
			case kb:
				score = KBKx(score, key2);
				break;
			case kbn:
				score = KBNKx(score, key2, flip ? side ^ 1 : side);
				break;
			case knn:
				score = KNNKx(score, key2);
				break;
			case k:
			case kn:
				score = min(0, score);
				break;
			default:
				break;
		}
		if ((key1 & ~all_pawns) == kb && (key2 & ~all_pawns) == kb) {
			score = KBxKBy(eval, key1, key2);
		}
		return flip ? -score : score;
	}

	__forceinline int KBxKBy(int eval, uint32 key1, uint32 key2) {
		if (sameColor(lsb(board->bishops(0)), lsb(board->bishops(1)))) {
			return eval;
		}
		if (abs(pawnCount(0) - pawnCount(1)) <= 2) {
			return eval/2;
		}
		return eval;
	}

	__forceinline int KRNKx(int eval, uint32 key2) {
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

	__forceinline int KRBKx(int eval, uint32 key2) {
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

	__forceinline int KBBKx(int eval, uint32 key2) {
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

	__forceinline int KBNKx(int eval, uint32 key2, int side1) {
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

	__forceinline int KNNKx(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case k:
			case kn:
				if (key2 & all_pawns) return min(0, eval); 
				else return 0;
			default:
				break;
		}
		return eval;
	}


	__forceinline int KBKx(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case knn:
				if ((key2 & all_pawns) == 0) return eval/8; 
				break;
			default:
				break;
		}
		return min(0, eval);
	}

	__forceinline int KRKx(int eval, uint32 key2) {
		switch (key2 & ~all_pawns) {
			case kb:
			case kn:
				if (key2 & all_pawns) return min(eval/4, eval); 
				return eval/4;
			case kbb:
			case kbn:
			case knn:
				if ((key2 & all_pawns) == 0) return eval/8; 
				break;
			default:
				break;
		}
		return eval;
	}

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
