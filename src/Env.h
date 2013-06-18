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

// Windows
#if 1 || defined(WIN32) || defined(WIN64)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 // Windows Vista.
#endif

#include <windows.h>
#include <sys/timeb.h>

#ifdef _MSC_VER

#include <intrin.h>

#pragma warning(disable : 4706) // assignment within conditional expression
#pragma warning(disable : 4100) // unreferenced formal parameters
#pragma warning(disable : 4127) // conditional expression is constant
//#pragma warning(disable : 4505)
//#pragma warning(disable : 4701)
#pragma warning(disable : 4996) // This function or variable may be unsafe. Consider using strtok_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details
typedef unsigned __int64 uint64;
typedef unsigned __int32 uint32;
typedef unsigned __int16 uint16;
typedef unsigned __int8 uint8;
typedef __int16 int16;
typedef unsigned int uint;
#define snprintf _snprintf
#else
#include <windows.h>
#include <sys/timeb.h>
#include <x86intrin.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#define __USE_MINGW_ANSI_STDIO 1

//#define stricmp strcasecmp
//#define strnicmp strncasecmp

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int16_t int16;
typedef unsigned int uint;
//#define __forceinline inline

#endif

#endif // Windows

#include <stdio.h>
#include <iomanip>
#include <time.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <direct.h>

//using namespace std;
