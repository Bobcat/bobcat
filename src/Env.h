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
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <sys/timeb.h>
#include <intrin.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#define __USE_MINGW_ANSI_STDIO 1

#include <algorithm>
#include <conio.h>
#include <math.h>
#include <assert.h>

#ifdef __forceinline
#undef __forceinline
#endif
#define __forceinline __attribute__((always_inline)) inline

