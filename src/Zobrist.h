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
namespace zobrist
{

uint64_t zobrist_pcsq[14][64];
uint64_t zobrist_castling[16];
uint64_t zobrist_side;
uint64_t zobrist_ep_file[8];

static void initialize() {
  std::mt19937_64 prng64;
  prng64.seed(std::mt19937_64::default_seed);

  for (int p = Pawn; p <= King; p++) {
    for (int sq = 0; sq < 64; sq++) {
      zobrist_pcsq[p][sq] = prng64();
      zobrist_pcsq[p + 8][sq] = prng64();
    }
  }
  for (int i = 0; i < 16; i++) {
    zobrist_castling[i] = prng64();
  }
  for (int i = 0; i < 8; i++) {
    zobrist_ep_file[i] = prng64();
  }
  zobrist_side = prng64();
}

}//namespace zobrist

using namespace zobrist;

