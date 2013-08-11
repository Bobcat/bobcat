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
#include <windows.h>
#include <sys/timeb.h>
#include <intrin.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#define __USE_MINGW_ANSI_STDIO 1

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int16_t int16;
typedef unsigned int uint;

#include <stdio.h>
#include <iomanip>
#include <time.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <direct.h>
#include <sys/time.h>

#ifdef __forceinline
#undef __forceinline
#endif
#define __forceinline __attribute__((always_inline)) inline

typedef int Score;
typedef int Depth;

