// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// DESCRIPTION:
//	Simple basic typedefs, isolated here to make it easier
//	 separating modules.
//    
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

// Windows CE is missing some vital ANSI C functions.  We have to
// use our own replacements.

#ifdef _WIN32_WCE
#include "libc_wince.h"
#endif

// C99 integer types; with gcc we just use this.  Other compilers 
// should add conditional statements that define the C99 types.

// What is really wanted here is stdint.h; however, some old versions
// of Solaris don't have stdint.h and only have inttypes.h (the 
// pre-standardisation version).  inttypes.h is also in the C99 
// standard and defined to include stdint.h, so include this. 

#include <inttypes.h>

#ifdef __cplusplus

// Use builtin bool type with C++.

typedef bool boolean;

#else

typedef enum 
{
    false, 
    true
} boolean;

#endif

typedef uint8_t byte;

#include <limits.h>

#ifdef _WIN32

#define DIR_SEPARATOR '\\'
#define PATH_SEPARATOR ';'

#else

#define DIR_SEPARATOR '/'
#define PATH_SEPARATOR ':'

#endif

#define arrlen(array) (sizeof(array) / sizeof(*array))

// Keywords
#if defined(__GNUC__)
	#define __OBSIDIAN_INLINE inline
	#define __OBSIDIAN_FORCEINLINE __attribute__((always_inline))
	#define __OBSIDIAN_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
	#define __OBSIDIAN_INLINE _inline
	#define __OBSIDIAN_FORCEINLINE __forceinline
	#define __OBSIDIAN_UNUSED
#else
	#define __OBSIDIAN_INLINE inline
	#define __OBSIDIAN_FORCEINLINE
	#define __OBSIDIAN_UNUSED
#endif

/***************************
*** DATA READING/WRITING ***
***************************/

#define BP_MERGE(a,b) a##b

#if defined(__arm__) || defined(_M_ARM) || defined(__sparc__) || defined(__sparc)
	/* Access to pointer data for system that can't handle unaligned access */
	// Lets say we have the following data:
	// { 01  23  45  67  |  89  AB  CD  EF }
	//      [*DEREF           ]
	// On normal systems we can just dereference as normal, but on some systems
	// such as ARM, we cannot do this. Instead we have to dereference both sides
	// then merge the data together.
	// Or we could just read byte by byte.

	#define BP_READ(w,x) static __OBSIDIAN_INLINE x __OBSIDIAN_FORCEINLINE BP_MERGE(Read,w)(const x** const Ptr)\
	{\
		x Ret = 0;\
		uint8_t* p8;\
		size_t i;\
		\
		if (!Ptr || !(*Ptr))\
			return 0;\
		\
		p8 = (uint8_t*)*Ptr;\
		for (i = 0; i < sizeof(x); i++)\
			((uint8_t*)&Ret)[i] = p8[i];\
		\
		(*Ptr)++;\
		return Ret;\
	}
#else
	/* Normal Pointer Access */
	#define BP_READ(w,x) static __OBSIDIAN_INLINE x __OBSIDIAN_FORCEINLINE BP_MERGE(Read,w)(const x** const Ptr)\
	{\
		x Ret;\
		\
		if (!Ptr || !(*Ptr))\
			return 0;\
		\
		Ret = **Ptr;\
		(*Ptr)++;\
		return Ret;\
	}
#endif

BP_READ(Int8,int8_t)
BP_READ(Int16,int16_t)
BP_READ(Int32,int32_t)
BP_READ(UInt8,uint8_t)
BP_READ(UInt16,uint16_t)
BP_READ(UInt32,uint32_t)

#define BP_WRITE(w,x) static __OBSIDIAN_INLINE void __OBSIDIAN_FORCEINLINE BP_MERGE(Write,w)(x** const Ptr, const x Val)\
{\
	if (!Ptr || !(*Ptr))\
		return;\
	**Ptr = Val;\
	(*Ptr)++;\
}

BP_WRITE(Int8,int8_t)
BP_WRITE(Int16,int16_t)
BP_WRITE(Int32,int32_t) 
BP_WRITE(UInt8,uint8_t)
BP_WRITE(UInt16,uint16_t)
BP_WRITE(UInt32,uint32_t)

#undef BP_READ
#undef BP_WRITE

#endif

