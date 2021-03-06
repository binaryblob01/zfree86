/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_dga.c,v 1.10 2004/12/07 15:59:20 tsi Exp $ */

/*
 * Copyright (C) 1994-2000 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * file: s3v_dga.c
 * ported from mga
 *
 */


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "s3v.h"
#if 0
#include "mga_bios.h"
#include "mga.h"
#include "mga_reg.h"
#include "mga_macros.h"
#endif
#include "dgaproc.h"


static Bool S3V_OpenFramebuffer(ScrnInfoPtr, char **, unsigned int *, 
				unsigned int *, unsigned int *, unsigned int *);
static Bool S3V_SetMode(ScrnInfoPtr, DGAModePtr);
static int  S3V_GetViewport(ScrnInfoPtr);
static void S3V_SetViewport(ScrnInfoPtr, int, int, int);
static void S3V_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void S3V_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
/* dummy... */
#if 0
static void MGA_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
			      unsigned long);
#endif

static
DGAFunctionRec S3V_DGAFuncs = {
   S3V_OpenFramebuffer,
   NULL,
   S3V_SetMode,
   S3V_SetViewport,
   S3V_GetViewport,
   S3VAccelSync,
   S3V_FillRect,
   S3V_BlitRect,
   NULL
   /* dummy... MGA_BlitTransRect */
};


Bool
S3VDGAInit(ScreenPtr pScreen)
{   
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   S3VPtr ps3v = S3VPTR(pScrn);
   DGAModePtr modes = NULL, newmodes = NULL, currentMode;
   DisplayModePtr pMode, firstMode;
   int Bpp = pScrn->bitsPerPixel >> 3;
   int num = 0;
   Bool oneMore;

   PVERB5("	S3VDGAInit\n");
    
   pMode = firstMode = pScrn->modes;

   while(pMode) {
	/* The MGA driver wasn't designed with switching depths in
	   mind.  Subsequently, large chunks of it will probably need
	   to be rewritten to accommodate depth changes in DGA mode */

	if(0 /*pScrn->displayWidth != pMode->HDisplay*/) {
	    newmodes = xrealloc(modes, (num + 2) * sizeof(DGAModeRec));
	    oneMore = TRUE;
	} else {
	    newmodes = xrealloc(modes, (num + 1) * sizeof(DGAModeRec));
	    oneMore = FALSE;
	}

	if(!newmodes) {
	   xfree(modes);
	   return FALSE;
	}
	modes = newmodes;

SECOND_PASS:

	currentMode = modes + num;
	num++;

	currentMode->mode = pMode;
	currentMode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	if(!ps3v->NoAccel)
	   currentMode->flags |= DGA_FILL_RECT | DGA_BLIT_RECT;
	if(pMode->Flags & V_DBLSCAN)
	   currentMode->flags |= DGA_DOUBLESCAN;
	if(pMode->Flags & V_INTERLACE)
	   currentMode->flags |= DGA_INTERLACED;
	currentMode->byteOrder = pScrn->imageByteOrder;
	currentMode->depth = pScrn->depth;
	currentMode->bitsPerPixel = pScrn->bitsPerPixel;
	currentMode->red_mask = pScrn->mask.red;
	currentMode->green_mask = pScrn->mask.green;
	currentMode->blue_mask = pScrn->mask.blue;
	currentMode->visualClass = (Bpp == 1) ? PseudoColor : TrueColor;
	currentMode->viewportWidth = pMode->HDisplay;
	currentMode->viewportHeight = pMode->VDisplay;
	/* currentMode->xViewportStep = (3 - ps3v->BppShift); */
				/* always 1 on ViRGE ? */
	currentMode->xViewportStep = 1;
	currentMode->yViewportStep = 1;
	currentMode->viewportFlags = DGA_FLIP_RETRACE;
	/* currentMode->offset = ps3v->YDstOrg * (pScrn->bitsPerPixel / 8);
	 * MGA, 0 for ViRGE */
	currentMode->offset = 0;
	/* currentMode->address = pMga->FbStart;   MGA */
	currentMode->address = ps3v->FBBase;
/*cep*/
  xf86ErrorFVerb(VERBLEV, 
	"	S3VDGAInit firstone vpWid=%d, vpHgt=%d, Bpp=%d, mdbitsPP=%d\n",
		currentMode->viewportWidth,
		currentMode->viewportHeight,
		Bpp,
		currentMode->bitsPerPixel
		 );
		

	if(oneMore) { /* first one is narrow width */
	    currentMode->bytesPerScanline = ((pMode->HDisplay * Bpp) + 3) & ~3L;
	    currentMode->imageWidth = pMode->HDisplay;
	    /* currentMode->imageHeight =  pMga->FbUsableSize /
					currentMode->bytesPerScanline; 
					MGA above */
	    currentMode->imageHeight =  pMode->VDisplay;
	    currentMode->pixmapWidth = currentMode->imageWidth;
	    currentMode->pixmapHeight = currentMode->imageHeight;
	    currentMode->maxViewportX = currentMode->imageWidth - 
					currentMode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    currentMode->maxViewportY = currentMode->imageHeight -
					currentMode->viewportHeight;
	    oneMore = FALSE;

/*cep*/
  xf86ErrorFVerb(VERBLEV, 
	"	S3VDGAInit imgHgt=%d, ram=%d, bytesPerScanl=%d\n",
		currentMode->imageHeight,
		ps3v->videoRambytes,
		currentMode->bytesPerScanline );
		
	    goto SECOND_PASS;
	} else {
	    currentMode->bytesPerScanline = 
			((pScrn->displayWidth * Bpp) + 3) & ~3L;
	    currentMode->imageWidth = pScrn->displayWidth;
	    /* currentMode->imageHeight =  pMga->FbUsableSize /
					currentMode->bytesPerScanline; 
					*/
	    currentMode->imageHeight =  ps3v->videoRambytes /
	    			currentMode->bytesPerScanline;
	    currentMode->pixmapWidth = currentMode->imageWidth;
	    currentMode->pixmapHeight = currentMode->imageHeight;
	    currentMode->maxViewportX = currentMode->imageWidth - 
					currentMode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    currentMode->maxViewportY = currentMode->imageHeight -
					currentMode->viewportHeight;
	}	    

	pMode = pMode->next;
	if(pMode == firstMode)
	   break;
   }

   ps3v->numDGAModes = num;
   ps3v->DGAModes = modes;

   return DGAInit(pScreen, &S3V_DGAFuncs, modes, num);  
}


static Bool
S3V_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   static int OldDisplayWidth[MAXSCREENS];
   int index = pScrn->pScreen->myNum;

   S3VPtr ps3v = S3VPTR(pScrn);

   if(!pMode) { /* restore the original mode */
	/* put the ScreenParameters back */
	
	pScrn->displayWidth = OldDisplayWidth[index];
	
        S3VSwitchMode(index, pScrn->currentMode, 0);
	ps3v->DGAactive = FALSE;
   } else {
	if(!ps3v->DGAactive) {  /* save the old parameters */
	    OldDisplayWidth[index] = pScrn->displayWidth;

	    ps3v->DGAactive = TRUE;
	}

	pScrn->displayWidth = pMode->bytesPerScanline / 
			      (pMode->bitsPerPixel >> 3);

        S3VSwitchMode(index, pMode->mode, 0);
   }
   
   return TRUE;
}



static int  
S3V_GetViewport(
  ScrnInfoPtr pScrn
){
    S3VPtr ps3v = S3VPTR(pScrn);

    return ps3v->DGAViewportStatus;
}

static void 
S3V_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   S3VPtr ps3v = S3VPTR(pScrn);

   S3VAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
   ps3v->DGAViewportStatus = 0;  /* MGAAdjustFrame loops until finished */
}

static void 
S3V_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    S3VPtr ps3v = S3VPTR(pScrn);

    if(ps3v->AccelInfoRec) {
	(*ps3v->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*ps3v->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(ps3v->AccelInfoRec);
    }
}

static void 
S3V_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    S3VPtr ps3v = S3VPTR(pScrn);

    if(ps3v->AccelInfoRec) {
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	(*ps3v->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	(*ps3v->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);
	SET_SYNC_FLAG(ps3v->AccelInfoRec);
    }
}

#if 0
static void 
MGA_BlitTransRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty,
   unsigned long color
){
  /* this one should be separate since the XAA function would
     prohibit usage of ~0 as the key */
}
#endif

static Bool 
S3V_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned int *mem,
   unsigned int *size,
   unsigned int *offset,
   unsigned int *flags
){
    S3VPtr ps3v = S3VPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = ps3v->PciInfo->memBase[0];
    *size = ps3v->videoRambytes;
    *offset = 0;
    *flags = 0;

    return TRUE;
}
