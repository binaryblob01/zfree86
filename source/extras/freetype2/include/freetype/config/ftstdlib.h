/* $XFree86: xc/extras/freetype2/include/freetype/config/ftstdlib.h,v 1.2 2007/05/18 18:01:57 tsi Exp $ */

/***************************************************************************/
/*                                                                         */
/*  ftstdlib.h                                                             */
/*                                                                         */
/*    ANSI-specific library and header configuration file (specification   */
/*    only).                                                               */
/*                                                                         */
/*  Copyright 2002, 2003, 2004 by                                          */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* This file is used to group all #includes to the ANSI C library that   */
  /* FreeType normally requires.  It also defines macros to rename the     */
  /* standard functions within the FreeType source code.                   */
  /*                                                                       */
  /* Load a file which defines __FTSTDLIB_H__ before this one to override  */
  /* it.                                                                   */
  /*                                                                       */
  /*************************************************************************/


#ifndef __FTSTDLIB_H__
#define __FTSTDLIB_H__


  /**********************************************************************/
  /*                                                                    */
  /*                           integer limits                           */
  /*                                                                    */
  /* UINT_MAX and ULONG_MAX are used to automatically compute the size  */
  /* of `int' and `long' in bytes at compile-time.  So far, this works  */
  /* for all platforms the library has been tested on.                  */
  /*                                                                    */
  /* Note that on the extremely rare platforms that do not provide      */
  /* integer types that are _exactly_ 16 and 32 bits wide (e.g. some    */
  /* old Crays where `int' is 36 bits), we do not make any guarantee    */
  /* about the correct behaviour of FT2 with all fonts.                 */
  /*                                                                    */
  /* In these case, "ftconfig.h" will refuse to compile anyway with a   */
  /* message like "couldn't find 32-bit type" or something similar.     */
  /*                                                                    */
  /* IMPORTANT NOTE: We do not define aliases for heap management and   */
  /*                 i/o routines (i.e. malloc/free/fopen/fread/...)    */
  /*                 since these functions should all be encapsulated   */
  /*                 by platform-specific implementations of            */
  /*                 "ftsystem.c".                                      */
  /*                                                                    */
  /**********************************************************************/


#ifndef FONTMODULE

#include <limits.h>

#define FT_UINT_MAX   UINT_MAX
#define FT_ULONG_MAX  ULONG_MAX


  /**********************************************************************/
  /*                                                                    */
  /*                 character and string processing                    */
  /*                                                                    */
  /**********************************************************************/


#include <ctype.h>

#define ft_isalnum   isalnum
#define ft_isupper   isupper
#define ft_islower   islower
#define ft_isdigit   isdigit
#define ft_isxdigit  isxdigit


#include <string.h>

#define ft_memcmp   memcmp
#define ft_memcpy   memcpy
#define ft_memmove  memmove
#define ft_memset   memset
#define ft_strcat   strcat
#define ft_strcmp   strcmp
#define ft_strcpy   strcpy
#define ft_strlen   strlen
#define ft_strncmp  strncmp
#define ft_strncpy  strncpy
#define ft_strrchr  strrchr


#include <stdio.h>

#define ft_sprintf  sprintf


  /**********************************************************************/
  /*                                                                    */
  /*                             sorting                                */
  /*                                                                    */
  /**********************************************************************/


#include <stdlib.h>

#define ft_qsort  qsort
#define ft_exit   exit    /* only used to exit from unhandled exceptions */

#define ft_atol   atol


  /**********************************************************************/
  /*                                                                    */
  /*                         execution control                          */
  /*                                                                    */
  /**********************************************************************/


#include <setjmp.h>

#define ft_jmp_buf  jmp_buf   /* note: this cannot be a typedef since */
                              /*       jmp_buf is defined as a macro  */
                              /*       on certain platforms           */

#define ft_setjmp   setjmp    /* same thing here */
#define ft_longjmp  longjmp   /* "               */


#else

#include <X11/Xmd.h>
#define _XTYPEDEF_BOOL
#include <X11/Xdefs.h>
#define DONT_DEFINE_WRAPPERS
#define DEFINE_SETJMP_WRAPPERS
#include "xf86_ansic.h"
#undef DONT_DEFINE_WRAPPERS

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((xf86size_t)&((TYPE*)0)->MEMBER)
#endif

#define FT_CHAR_BIT  8
#define FT_UINT_MAX  4294967295U
#ifdef LONG64
#define FT_ULONG_MAX 18446744073709551615UL
#else
#define FT_ULONG_MAX 4294967295UL
#endif

#define ft_isalnum   xf86isalnum
#define ft_isupper   xf86isupper
#define ft_islower   xf86islower
#define ft_isdigit   xf86isdigit
#define ft_isxdigit  xf86isxdigit

#define ft_memcmp    xf86memcmp
#define ft_memcpy    xf86memcpy
#define ft_memmove   xf86memmove
#define ft_memset    xf86memset
#define ft_strcat    xf86strcat
#define ft_strcmp    xf86strcmp
#define ft_strcpy    xf86strcpy
#define ft_strlen    xf86strlen
#define ft_strncmp   xf86strncmp
#define ft_strncpy   xf86strncpy
#define ft_strrchr   xf86strrchr

#define ft_sprintf   xf86sprintf

#define ft_qsort     xf86qsort
#define ft_exit      xf86exit

#define ft_atol      xf86atol

#define ft_jmp_buf   jmp_buf
#define ft_setjmp    setjmp
#define ft_longjmp   longjmp

#undef  exit
#define exit         xf86exit

#undef  fprintf
#define fprintf      xf86fprintf

#undef  memcpy
#define memcpy       xf86memcpy
#undef  memset
#define memset       xf86memset

#undef  stderr
#define stderr       xf86stderr

#endif /* FONTMODULE */


  /* the following is only used for debugging purposes, i.e. when */
  /* FT_DEBUG_LEVEL_ERROR or FT_DEBUG_LEVEL_TRACE are defined     */
  /*                                                              */
#include <stdarg.h>


#endif /* __FTSTDLIB_H__ */


/* END */
