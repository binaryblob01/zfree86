/*
(c) Copyright 1996 Hewlett-Packard Company
(c) Copyright 1996 International Business Machines Corp.
(c) Copyright 1996 Sun Microsystems, Inc.
(c) Copyright 1996 Novell, Inc.
(c) Copyright 1996 Digital Equipment Corp.
(c) Copyright 1996 Fujitsu Limited
(c) Copyright 1996 Hitachi, Ltd.

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
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of the copyright holders shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from said
copyright holders.
*/
/*
 * Copyright (c) 1996-2006 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/Xserver/Xprint/ddxInit.c,v 1.21 2007/04/03 00:21:07 tsi Exp $ */

#include <X11/X.h>
#include <X11/Xproto.h>
#include "windowstr.h"
#include "servermd.h"
#include <X11/Xos.h>
#include "DiPrint.h"
#include "mipointer.h"

/*
 *-----------------------------------------------------------------------
 * InitOutput --
 *	If this is built as a print-only server, then we must supply
 *      an InitOutput routine.  If a normal server's real ddx InitOutput
 *      is used, then it should call PrinterInitOutput if it so desires.
 *      The ddx-level hook is needed to allow the printer stuff to 
 *      create additional screens.  An extension can't reliably do
 *      this for two reasons:
 *
 *          1) If InitOutput doesn't create any screens, then main()
 *             exits before calling InitExtensions().
 *
 *          2) Other extensions may rely on knowing about all screens
 *             when they initialize, and we can't guarantee the order
 *             of extension initialization.
 *
 * Results:
 *	ScreenInfo filled in, and PrinterInitOutput is called to create
 *      the screens associated with printers.
 *
 * Side Effects:
 *	None
 *
 *-----------------------------------------------------------------------
 */

void 
InitOutput(
    ScreenInfo 	  *pScreenInfo,
    const int     argc,
    const char    **argv)
{
    pScreenInfo->imageByteOrder = IMAGE_BYTE_ORDER;
    pScreenInfo->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
    pScreenInfo->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
    pScreenInfo->bitmapBitOrder = BITMAP_BIT_ORDER;

    pScreenInfo->numPixmapFormats = 0; /* get them in PrinterInitOutput */
    screenInfo.numVideoScreens = 0;
    PrinterInitOutput(pScreenInfo, argc, argv);
}

static void
BellProc(
    int volume,
    DeviceIntPtr pDev)
{
    return;
}

static void
KeyControlProc(
    DeviceIntPtr pDev,
    KeybdCtrl *ctrl)
{
    return;
}

static KeySym printKeyMap[256];
static CARD8 printModMap[256];

static int
KeyboardProc(
    DevicePtr pKbd,
    int what)
{
    KeySymsRec keySyms;

    keySyms.minKeyCode = 8;
    keySyms.maxKeyCode = 8;
    keySyms.mapWidth = 1;
    keySyms.map = printKeyMap;

    switch(what)
    {
	case DEVICE_INIT:
	    InitKeyboardDeviceStruct(pKbd, &keySyms, printModMap, 
				     (BellProcPtr)BellProc,
				     KeyControlProc);
	    break;
	case DEVICE_ON:
	    break;
	case DEVICE_OFF:
	    break;
	case DEVICE_CLOSE:
	    break;
    }
    return Success;
}

static int
PointerProc(
     DevicePtr pPtr,
     int what)
{
#define NUM_BUTTONS 1
    CARD8 map[NUM_BUTTONS];

    switch(what)
      {
        case DEVICE_INIT:
	  {
	      map[0] = 0;
	      InitPointerDeviceStruct(pPtr, map, NUM_BUTTONS, 
				      miPointerGetMotionEvents, 
				      (PtrCtrlProcPtr)_XpVoidNoop,
				      miPointerGetMotionBufferSize());
	      break;
	  }
        case DEVICE_ON:
	  break;
        case DEVICE_OFF:
	  break;
        case DEVICE_CLOSE:
	  break;
      }
    return Success;
}

void
InitInput(
     const int	argc,
     const char **argv)
{
    DeviceIntPtr ptr, kbd;

    ptr = AddInputDevice((DeviceProc)PointerProc, TRUE);
    kbd = AddInputDevice((DeviceProc)KeyboardProc, TRUE);
    RegisterPointerDevice(ptr);
    RegisterKeyboardDevice(kbd);
    return;
}


Bool
LegalModifier(
     unsigned int key,
     DevicePtr dev)
{
    return TRUE;
}

void
ProcessInputEvents(void)
{
}

#ifdef __DARWIN__
void
DarwinHandleGUI(int argc, const char *argv[], char *envp[])
{
}
#endif

#ifdef DDXOSINIT
void
OsVendorPreInit(void)
{
}

void
OsVendorInit(void)
{
}
#endif

#ifdef DDXOSFATALERROR
void
OsVendorFatalError(void)
{
}
#endif

#ifdef DDXTIME
CARD32
GetTimeInMillis(void)
{
    struct timeval  tp;

    X_GETTIMEOFDAY(&tp);
    return(tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}
#endif

/****************************************
* ddxUseMsg()
*
* Called my usemsg from os/utils/c
*
*****************************************/

void ddxUseMsg(void)
{
	/* Right now, let's just do nothing */
}

void AbortDDX (void)
{
}

void ddxGiveUp(void)	/* Called by GiveUp() */
{
}

int
ddxProcessArgument (
    int argc,
    const char *argv[],
    int i)
{
    return XprintOptions(argc, argv, i) - i;
}

#ifdef AIXV3
/*
 * This is just to get the server to link on AIX, where some bits
 * that should be in os/ are instead in hw/ibm.
 */
int SelectWaitTime = 10000; /* usec */
#endif
