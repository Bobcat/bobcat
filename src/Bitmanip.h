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

// Currently only setup for Windows platform / MSVC 2008 Express 64 bits builds.

#include <intrin.h>

__forceinline void reset_lsb(uint64& x) {
	x &= (x - 1);
}

__forceinline int pop_count(uint64 x) { 
	int count = 0;
	while (x) {
		count++;
		reset_lsb(x);
	}
	return count;
}

static int msb_table[256];
__forceinline int msb(uint64 x) { // E.N. method 
	int result = 0;
	if (x > 0xFFFFFFFF) {
		x >>= 32;
		result = 32;
	}
	if (x > 0xFFFF) {
		x >>= 16;
		result += 16;
	}
	if (x > 0xFF) {
		x >>= 8;
		result += 8;
	}
	return result + msb_table[x];
}

__forceinline int lsb(uint64 x) { 
	DWORD index;
	_BitScanForward64(&index, x);
	return index;
}

void Bitmanip_h_initialise() {
	for (int i = 0; i < 256; i++) {
		msb_table[i] = 0;
		int b = i >> 1;
		while (b) {
			msb_table[i]++;
			b >>= 1;
		}
	}
}
