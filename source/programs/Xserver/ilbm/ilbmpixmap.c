/* $XFree86: xc/programs/Xserver/ilbm/ilbmpixmap.c,v 3.3 2006/01/09 15:00:32 dawes Exp $ */
/***********************************************************

Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* pixmap management
   written by drewry, september 1986

   on a monchrome device, a pixmap is a bitmap.
*/

/* Modified jun 95 by Geert Uytterhoeven (Geert.Uytterhoeven@cs.kuleuven.ac.be)
   to use interleaved bitplanes instead of normal bitplanes */

#include <X11/Xmd.h>
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "maskbits.h"

#include "ilbm.h"
#include "mi.h"

#include "servermd.h"

PixmapPtr
ilbmCreatePixmap(pScreen, width, height, depth)
	ScreenPtr		pScreen;
	int				width;
	int				height;
	int				depth;
{
	PixmapPtr pPixmap;
	int datasize;
	int paddedWidth;

	if ((width > MAXSHORT) || (height > MAXSHORT))
		return NullPixmap;

	paddedWidth = BitmapBytePad(width);
	datasize = height * paddedWidth * depth;
	pPixmap = AllocatePixmap(pScreen, datasize);
	if (!pPixmap)
		return(NullPixmap);
	pPixmap->drawable.type = DRAWABLE_PIXMAP;
	pPixmap->drawable.class = 0;
	pPixmap->drawable.pScreen = pScreen;
	pPixmap->drawable.depth = depth;
	pPixmap->drawable.bitsPerPixel = depth;
	pPixmap->drawable.id = 0;
	pPixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;
	pPixmap->drawable.x = 0;
	pPixmap->drawable.y = 0;
	pPixmap->drawable.width = width;
	pPixmap->drawable.height = height;
	pPixmap->devKind = paddedWidth;
	pPixmap->refcnt = 1;
#ifdef PIXPRIV
	pPixmap->devPrivate.ptr =  datasize ?
				(pointer)((char *)pPixmap + pScreen->totalPixmapSize) : NULL;
#else
	pPixmap->devPrivate.ptr = (pointer)(pPixmap + 1);
#endif
	return(pPixmap);
}

Bool
ilbmDestroyPixmap(pPixmap)
	PixmapPtr pPixmap;
{
	if (--pPixmap->refcnt)
		return(TRUE);
	xfree(pPixmap);
	return(TRUE);
}


PixmapPtr
ilbmCopyPixmap(pSrc)
	register PixmapPtr pSrc;
{
	register PixmapPtr pDst;
	int size;
	ScreenPtr pScreen;

	size = pSrc->drawable.height * pSrc->devKind * pSrc->drawable.depth;
	pScreen = pSrc->drawable.pScreen;
	pDst = (*pScreen->CreatePixmap)(pScreen, pSrc->drawable.width,
											  pSrc->drawable.height, pSrc->drawable.depth);
	if (!pDst)
		return(NullPixmap);
	memmove((char *)pDst->devPrivate.ptr, (char *)pSrc->devPrivate.ptr, size);
	return(pDst);
}


/* replicates a pattern to be a full 32 bits wide.
   relies on the fact that each scnaline is longword padded.
   doesn't do anything if pixmap is not a factor of 32 wide.
   changes width field of pixmap if successful, so that the fast
		XRotatePixmap code gets used if we rotate the pixmap later.

   calculate number of times to repeat
   for each scanline of pattern
	  zero out area to be filled with replicate
	  left shift and or in original as many times as needed
*/
void
ilbmPadPixmap(pPixmap)
	PixmapPtr pPixmap;
{
	register int width = pPixmap->drawable.width;
	register int h;
	register PixelType mask;
	register PixelType *p;
	register PixelType bits;		/* real pattern bits */
	register int i;
	int d;
	int rep;						/* repeat count for pattern */

	if (width >= PPW)
		return;

	rep = PPW/width;
	if (rep*width != PPW)
		return;

	mask = endtab[width];

	p = (PixelType *)(pPixmap->devPrivate.ptr);

	for (d = 0; d < pPixmap->drawable.depth; d++) {
		for (h = 0; h < pPixmap->drawable.height; h++) {
			*p &= mask;
			bits = *p;
			for (i = 1; i < rep; i++) {
				bits = SCRRIGHT(bits, width);
				*p |= bits;
			}
			p++;
		}
	}
	pPixmap->drawable.width = PPW;
}

/* Rotates pixmap pPix by w pixels to the right on the screen. Assumes that
 * words are PPW bits wide, and that the least significant bit appears on the
 * left.
 */
void
ilbmXRotatePixmap(pPix, rw)
	PixmapPtr		pPix;
	register int rw;
{
	register PixelType		*pw, *pwFinal;
	register PixelType		t;

	if (pPix == NullPixmap)
		return;

	pw = (PixelType *)pPix->devPrivate.ptr;
	rw %= (int)pPix->drawable.width;
	if (rw < 0)
		rw += (int)pPix->drawable.width;
	if (pPix->drawable.width == PPW) {
		pwFinal = pw + pPix->drawable.height * pPix->drawable.depth;
		while (pw < pwFinal) {
			t = *pw;
			*pw++ = SCRRIGHT(t, rw) | (SCRLEFT(t, (PPW-rw)) & endtab[rw]);
		}
	} else {
		/* We no longer do this.  Validate doesn't try to rotate odd-size
		 * tiles or stipples.  ilbmUnnatural<tile/stipple>FS works directly off
		 * the unrotate tile/stipple in the GC
		 */
		ErrorF("X internal error: trying to rotate odd-sized pixmap.\n");
	}

}

/* Rotates pixmap pPix by h lines.  Assumes that h is always less than
   pPix->height
   works on any width.
 */
void
ilbmYRotatePixmap(pPix, rh)
	register PixmapPtr		pPix;
	int		rh;
{
	int nbyDown;		/* bytes to move down to row 0; also offset of
								row rh */
	int nbyUp;			/* bytes to move up to line rh; also
								offset of first line moved down to 0 */
	char *pbase;
	char *ptmp;
	int height;
	int aux;

	if (pPix == NullPixmap)
		return;
	height = (int) pPix->drawable.height;
	rh %= height;
	if (rh < 0)
		rh += height;

	aux = pPix->devKind*pPix->drawable.depth;
	nbyDown = rh*aux;
	nbyUp = height*aux-nbyDown;

	if (!(ptmp = (char *)ALLOCATE_LOCAL(nbyUp)))
		return;

	pbase = (char *)pPix->devPrivate.ptr;
	memmove(ptmp, pbase, nbyUp);					/* save the low rows */
	memmove(pbase, pbase+nbyUp, nbyDown);		/* slide the top rows down */
	memmove(pbase+nbyDown, ptmp, nbyUp);		/* move lower rows up to row rh */
	DEALLOCATE_LOCAL(ptmp);
}

void
ilbmCopyRotatePixmap(psrcPix, ppdstPix, xrot, yrot)
	register PixmapPtr psrcPix, *ppdstPix;
	int		xrot, yrot;
{
	register PixmapPtr pdstPix;

	if ((pdstPix = *ppdstPix) && (pdstPix->devKind == psrcPix->devKind) &&
		(pdstPix->drawable.height == psrcPix->drawable.height) &&
		(pdstPix->drawable.depth == psrcPix->drawable.depth)) {
		memmove((char *)pdstPix->devPrivate.ptr, (char *)psrcPix->devPrivate.ptr,
				  psrcPix->drawable.height * psrcPix->devKind *
				  psrcPix->drawable.depth);
		pdstPix->drawable.width = psrcPix->drawable.width;
		pdstPix->drawable.serialNumber = NEXT_SERIAL_NUMBER;
	} else {
		if (pdstPix)
			/* FIX XBUG 6168 */
			(*pdstPix->drawable.pScreen->DestroyPixmap)(pdstPix);
		*ppdstPix = pdstPix = ilbmCopyPixmap(psrcPix);
		if (!pdstPix)
			return;
	}
	ilbmPadPixmap(pdstPix);
	if (xrot)
		ilbmXRotatePixmap(pdstPix, xrot);
	if (yrot)
		ilbmYRotatePixmap(pdstPix, yrot);
}