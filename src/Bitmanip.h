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

#include <intrin.h>

__forceinline void reset_lsb(uint64& x) {
	x &= (x - 1);
}

__forceinline int popCount(uint64 x) { 
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

#if defined(_MSC_VER) && defined(_M_X64)
// by Microsoft
__forceinline int lsb(uint64 x) { 
	DWORD index;
	_BitScanForward64(&index, x);
	return index;
}
#elif defined(_MSC_VER) && defined(_M_X86)
// from Strelka
__forceinline int lsb(uint64 x) { 
	_asm { 
		mov  eax, dword ptr x[0]
		test eax, eax
		jz   f_hi
		bsf  eax, eax
		jmp  f_ret
f_hi:    
		bsf  eax, dword ptr x[4]
		add  eax, 20h
f_ret:
	}
}
#else
// by Matt Taylor, the fastest non intrinsic/assembly
const unsigned char lsb_64_table[64] = {
	63, 30,  3, 32, 59, 14, 11, 33,
	60, 24, 50,  9, 55, 19, 21, 34,
	61, 29,  2, 53, 51, 23, 41, 18,
	56, 28,  1, 43, 46, 27,  0, 35,
	62, 31, 58,  4,  5, 49, 54,  6,
	15, 52, 12, 40,  7, 42, 45, 16,
	25, 57, 48, 13, 10, 39,  8, 44,
	20, 47, 38, 22, 17, 37, 36, 26
};

__forceinline int lsb(uint64 x) {
	x ^= (x - 1);
	unsigned int folded = (int) x ^ (x >> 32);
	return lsb_64_table[folded * 0x78291ACF >> 26];
}
#endif

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
