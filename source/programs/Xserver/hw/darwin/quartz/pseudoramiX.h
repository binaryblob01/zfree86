/*
 * Minimal implementation of PanoramiX/Xinerama
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/pseudoramiX.h,v 1.4 2007/01/04 02:48:12 tsi Exp $ */

#ifndef _PSEUDORAMIX_H_
#define _PSEUDORAMIX_H_ 1

#include "extnsionst.h"

extern int noPseudoramiXExtension;

void PseudoramiXAddScreen(int x, int y, int w, int h);
void PseudoramiXExtensionInit(INITARGS);
void PseudoramiXResetScreens(void);

#endif
