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

__forceinline void resetLSB(uint64_t& x) {
  x &= (x - 1);
}

#ifndef BK_POPCOUNT
__forceinline int popCount(uint64_t x) {
  return __builtin_popcountll(x);
}
#else
__forceinline int popCount(uint64_t x) {
  int y = 0; // some say auto y = int{0} is always better:)
  while (x) {
    y++;
    resetLSB(x);
  }
  return y;
}
#endif

__forceinline int lsb(uint64_t x) {
  return __builtin_ctzll(x);
}
