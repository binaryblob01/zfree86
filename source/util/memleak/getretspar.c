/* $XFree86: xc/util/memleak/getretspar.c,v 1.5 2006/04/25 03:09:42 dawes Exp $ */
/*
 *
Copyright 1992, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

/* Trace up the stack and build a call history -- SPARC specific code */

#include "getstack.h"

/* hack -- flush out the register windows by recursing */

static void
flushWindows(int depth)
{
    if (depth == 0)
	return;
    flushWindows(depth - 1);
}

#ifndef __GNUC__
extern unsigned long *getStackPointer(void);
#endif
extern int main(int, char **);

void
getStackTrace(unsigned long *results, int max)
{
    unsigned long *sp;
    unsigned long *ra, mainCall;

    flushWindows(32);
#ifdef __GNUC__
    __asm__ volatile ("mov %%sp, %0" : "=r" (sp));
#else
    sp = getStackPointer();
#endif

    while (max) {
	/* sparc stack traces are easy -- chain up the saved FP/SP values */
	ra = (unsigned long *)sp[15];
	sp = (unsigned long *)sp[14];
	/* stop when we get the call to main */
	mainCall = ((((unsigned long)main) -
		     ((unsigned long)ra)) >> 2) | 0x40000000;
	if (!ra || ra[0] == mainCall) {
	    *results++ = 0;
	    break;
	}
	*results++ = (unsigned long)ra;
	max--;
    }
}
