/**************************************************************************

Copyright 2003 Eric Anholt
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/extras/Mesa/src/mesa/drivers/dri/sis/sis_tex.h,v 1.1.1.2 2004/06/10 14:23:16 alanh Exp $ */

/*
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 */

#ifndef __SIS_TEX_H__
#define __SIS_TEX_H__

#ifdef GLX_DIRECT_RENDERING

extern void sisInitTextureFuncs( struct dd_function_table *table );
extern void sisUpdateTextureState( GLcontext *ctx );

#endif /* GLX_DIRECT_RENDERING */
#endif /* __SIS_TEX_H__ */
