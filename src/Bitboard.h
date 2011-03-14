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

namespace bitboard { // because HFILE is already a typedef at this point

typedef uint64 BB;

const BB AFILE = 0x0101010101010101;
const BB HFILE = 0x8080808080808080;
const BB BFILE = 0x0202020202020202;
const BB GFILE = 0x4040404040404040;
const BB RANK1 = 0x00000000000000ff;
const BB RANK2 = 0x000000000000ff00;
const BB RANK3 = 0x0000000000ff0000;
const BB RANK4 = 0x00000000ff000000;
const BB RANK5 = 0x000000ff00000000;
const BB RANK6 = 0x0000ff0000000000;
const BB RANK7 = 0x00ff000000000000;
const BB RANK8 = 0xff00000000000000;

BB bb_square_array[64];
BB bb_rank_array[64];
BB bb_file_array[64];
BB bb_between[64][64];
BB passed_pawn_front_span[2][64];
BB pawn_front_span[2][64];
BB pawn_east_attack_span[2][64];
BB pawn_west_attack_span[2][64];
BB pawn_captures[128];
BB knight_attacks[64];
BB king_attacks[64];
BB colored[2];

#define bb_square(x) bb_square_array[x]
#define bb_rank(x) bb_rank_array[x]
#define bb_file(x) bb_file_array[x]

__forceinline BB northOne(const BB& bb) { return bb << 8; }
__forceinline BB southOne(const BB& bb) { return bb >> 8; }
__forceinline BB eastOne(const BB& bb) { return (bb & ~HFILE) << 1; }
__forceinline BB westOne(const BB& bb) { return (bb & ~AFILE) >> 1; }
__forceinline BB northEastOne(const BB& bb) { return (bb  & ~HFILE) << 9; }
__forceinline BB southEastOne(const BB& bb) { return (bb  & ~HFILE) >> 7; }
__forceinline BB southWestOne(const BB& bb) { return (bb  & ~AFILE) >> 9; }
__forceinline BB northWestOne(const BB& bb) { return (bb  & ~AFILE) << 7; }

__forceinline BB northFill(const BB& bb) {
	BB fill = bb; 
	fill |= (fill <<  8);
	fill |= (fill << 16);
	fill |= (fill << 32);
	return fill;
}
 
__forceinline BB southFill(const BB& bb) {
	BB fill = bb; 
	fill |= (fill >>  8);
	fill |= (fill >> 16);
	fill |= (fill >> 32);
	return fill;
}

void print_bb(const BB bb, const char* s = 0) {
	cout << endl << (s ? s : "") << endl;
	for (int rank = 7; rank >=0; rank--) {
		cout << rank + 1 << "  ";
		for (int file = 0; file <= 7; file++) {
			cout << (bb & bb_square((rank << 3) + file) ? "1 " : ". ");
		}
		cout << endl;
	}
	cout << endl << "   a b c d e f g h" << endl;
}

void initBetweenBitboards(const Square from, BB (*stepFunc)(const BB&), int step) {
	BB bb = stepFunc(bb_square(from));
	Square to = from + step;
	BB between = 0;
	while (bb) {
		bb_between[from][to] = between;
		between |= bb;
		bb = stepFunc(bb);
		to += step;
	}
}

void Bitboard_h_initialise() {
	colored[0] = colored[1] = 0;
	for (Square sq = a1; sq <= h8; sq++) {
		bb_square_array[sq] = (BB)1 << sq;
		bb_rank_array[sq] = RANK1 << (sq & 56);
		bb_file_array[sq] = AFILE << (sq & 7);
		colored[((9 * sq) & 8) == 0 ? 1 : 0] |= bb_square(sq);
	}

	for (Square sq = a1; sq <= h8; sq++) {
		pawn_front_span[0][sq] = northFill(northOne(bb_square(sq)));
		pawn_front_span[1][sq] = southFill(southOne(bb_square(sq)));
		pawn_east_attack_span[0][sq] = northFill(northEastOne(bb_square(sq)));
		pawn_east_attack_span[1][sq] = southFill(southEastOne(bb_square(sq)));
		pawn_west_attack_span[0][sq] = northFill(northWestOne(bb_square(sq)));
		pawn_west_attack_span[1][sq] = southFill(southWestOne(bb_square(sq)));
		passed_pawn_front_span[0][sq] = pawn_east_attack_span[0][sq] | pawn_front_span[0][sq] | pawn_west_attack_span[0][sq];
		passed_pawn_front_span[1][sq] = pawn_east_attack_span[1][sq] | pawn_front_span[1][sq] | pawn_west_attack_span[1][sq];

		for (Square to = a1; to < h8; to++) {
			bb_between[sq][to] = 0;
	  	}
		initBetweenBitboards(sq, northOne, 8);
		initBetweenBitboards(sq, northEastOne, 9);
		initBetweenBitboards(sq, eastOne, 1);
		initBetweenBitboards(sq, southEastOne, -7);
		initBetweenBitboards(sq, southOne, -8);
		initBetweenBitboards(sq, southWestOne, -9);
		initBetweenBitboards(sq, westOne, -1);
		initBetweenBitboards(sq, northWestOne, 7);

		pawn_captures[sq] = (bb_square(sq) & ~HFILE) << 9;
		pawn_captures[sq] |= (bb_square(sq) & ~AFILE) << 7;
		pawn_captures[sq + 64] = (bb_square(sq) & ~AFILE) >> 9;
		pawn_captures[sq + 64] |= (bb_square(sq) & ~HFILE) >> 7;
	
		knight_attacks[sq] = (bb_square(sq) & ~(AFILE | BFILE)) << 6; 
		knight_attacks[sq] |= (bb_square(sq) & ~AFILE) << 15;
		knight_attacks[sq] |= (bb_square(sq) & ~HFILE) << 17; 
		knight_attacks[sq] |= (bb_square(sq) & ~(GFILE | HFILE)) << 10;
		knight_attacks[sq] |= (bb_square(sq) & ~(GFILE | HFILE)) >> 6;
		knight_attacks[sq] |= (bb_square(sq) & ~HFILE) >> 15;
		knight_attacks[sq] |= (bb_square(sq) & ~AFILE) >> 17;
		knight_attacks[sq] |= (bb_square(sq) & ~(AFILE | BFILE)) >> 10;

		king_attacks[sq] = (bb_square(sq) & ~AFILE) >> 1;
		king_attacks[sq] |= (bb_square(sq) & ~AFILE) << 7;
		king_attacks[sq] |= bb_square(sq) << 8;
		king_attacks[sq] |= (bb_square(sq) & ~HFILE) << 9;
		king_attacks[sq] |= (bb_square(sq) & ~HFILE) << 1;
		king_attacks[sq] |= (bb_square(sq) & ~HFILE) >> 7;
		king_attacks[sq] |= bb_square(sq) >> 8;
		king_attacks[sq] |= (bb_square(sq) & ~AFILE) >> 9;
	}
}

__forceinline BB neighbourFiles(const BB& bb) {
	return northFill(southFill(westOne(bb) | eastOne(bb)));
}

BB (*pawnPush[2])(const BB&) = { northOne, southOne };
BB (*pawnEastAttacks[2])(const BB&) = { northWestOne, southWestOne };
BB (*pawnWestAttacks[2])(const BB&) = { northEastOne, southEastOne };
BB (*pawnFill[2])(const BB&) = { northFill, southFill };

const BB rank_3[2] = { RANK3, RANK6 };
const BB rank_7[2] = { RANK7, RANK2 };
const BB rank_6_and_7[2] = { RANK6|RANK7, RANK2|RANK3 };
const BB rank_7_and_8[2] = { RANK7|RANK8, RANK1|RANK2 };

const int pawn_push_dist[2] = { 8, -8 };
const int pawn_double_push_dist[2] = { 16, -16 };
const int pawn_west_attack_dist[2] = { 9, -7 };
const int pawn_east_attack_dist[2] = { 7, -9 };

} // namespace bitboard

using namespace bitboard;
