/* $XFree86: xc/programs/Xserver/hw/dmx/glxProxy/glxsingle.c,v 1.4 2006/02/08 02:34:10 dawes Exp $ */
/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: This software was created using the
** OpenGL(R) version 1.2.1 Sample Implementation published by SGI, but has
** not been independently verified as being compliant with the OpenGL(R)
** version 1.2.1 Specification.
*/

#include "dmx.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxfont.h"
#include "dmxcb.h"

#undef Xmalloc
#undef Xcalloc
#undef Xrealloc
#undef Xfree

#define NEED_REPLIES
#include "glxserver.h"
#include "glxsingle.h"
#include "glxext.h"
#include "g_disptab.h"
/* #include "g_disptab_EXT.h" */
#include "unpack.h"
#include "glxutil.h"

#include "GL/glxproto.h"

#ifdef PANORAMIX
#include "panoramiXsrv.h"
#endif

/*
 * GetReqSingle - this is the equivalent of GetReq macro
 *    from Xlibint.h but it does not set the reqType field (the opcode).
 *    this is because the GL single opcodes has different naming convension
 *    the other X opcodes (ie. X_GLsop_GetFloatv).
 */
#if (__STDC__ && !defined(UNIXCPP)) || defined(ANSICPP)
#define GetReqSingle(name, req) \
        WORD64ALIGN\
	if ((dpy->bufptr + SIZEOF(x##name##Req)) > dpy->bufmax)\
		_XFlush(dpy);\
	req = (x##name##Req *)(dpy->last_req = dpy->bufptr);\
	req->length = (SIZEOF(x##name##Req))>>2;\
	dpy->bufptr += SIZEOF(x##name##Req);\
	dpy->request++

#else  /* non-ANSI C uses empty comment instead of "##" for token concatenation */
#define GetReqSingle(name, req) \
        WORD64ALIGN\
	if ((dpy->bufptr + SIZEOF(x/**/name/**/Req)) > dpy->bufmax)\
		_XFlush(dpy);\
	req = (x/**/name/**/Req *)(dpy->last_req = dpy->bufptr);\
	req->length = (SIZEOF(x/**/name/**/Req))>>2;\
	dpy->bufptr += SIZEOF(x/**/name/**/Req);\
	dpy->request++
#endif

#define X_GLXSingle 0   /* needed by GetReqExtra */

static int swap_vec_element_size = 0;

static void SendSwappedReply( ClientPtr client,
                              xGLXSingleReply *reply, 
			      char *buf,
			      int   buf_size )
{
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_SWAP_SHORT(&reply->sequenceNumber);
   __GLX_SWAP_INT(&reply->length);
   __GLX_SWAP_INT(&reply->retval);
   __GLX_SWAP_INT(&reply->size);

   if ( (buf_size == 0) && (swap_vec_element_size > 0) ) {
      /*
       * the reply has single component - need to swap pad3
       */
      if (swap_vec_element_size == 2) {
	 __GLX_SWAP_SHORT(&reply->pad3);
      }
      else if (swap_vec_element_size == 4) {
	 __GLX_SWAP_INT(&reply->pad3);
	 __GLX_SWAP_INT(&reply->pad4);    /* some requests use also pad4
                                          * i.e GetConvolutionFilter
					  */
      }
      else if (swap_vec_element_size == 8) {
	 __GLX_SWAP_DOUBLE(&reply->pad3);
      }
   }
   else if ( (buf_size > 0) && (swap_vec_element_size > 0) ) {
      /*
       * the reply has vector of elements which needs to be swapped
       */
      int vsize = buf_size / swap_vec_element_size;
      char *p = buf;
      int i;

      for (i=0; i<vsize; i++) {
	 if (swap_vec_element_size == 2) {
	    __GLX_SWAP_SHORT(p);
	 }
	 else if (swap_vec_element_size == 4) {
	    __GLX_SWAP_INT(p);
	 }
	 else if (swap_vec_element_size == 8) {
	    __GLX_SWAP_DOUBLE(p);
	 }

	 p += swap_vec_element_size;
      }

      /*
       * swap pad words as well - for case that some single reply uses
       * them as well
       */
      __GLX_SWAP_INT(&reply->pad3);
      __GLX_SWAP_INT(&reply->pad4);
      __GLX_SWAP_INT(&reply->pad5);
      __GLX_SWAP_INT(&reply->pad6);

   }

    WriteToClient(client, sizeof(xGLXSingleReply),(char *)reply);
    if (buf_size > 0)
       WriteToClient(client, buf_size, (char *)buf);
}

int __glXForwardSingleReq( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   xGLXSingleReq *be_req;
    __GLXcontext *glxc;
   int from_screen = 0;
   int to_screen = 0;
   int buf_size;
   int s;

    glxc = __glXLookupContextByTag(cl, req->contextTag);
    if (!glxc) {
	return 0;
    }
    from_screen = to_screen = glxc->pScreen->myNum;

#ifdef PANORAMIX
    if (!noPanoramiXExtension) {
       from_screen = 0;
       to_screen = screenInfo.numScreens - 1;
    }
#endif

    pc += sz_xGLXSingleReq;
    buf_size = (req->length << 2) - sz_xGLXSingleReq;

    /*
     * just forward the request to back-end server(s)
     */
    for (s=from_screen; s<=to_screen; s++) {
       DMXScreenInfo *dmxScreen = &dmxScreens[s];
       Display *dpy = GetBackEndDisplay(cl,s);

       LockDisplay(dpy);
       GetReqSingle(GLXSingle,be_req);
       be_req->reqType = dmxScreen->glxMajorOpcode;
       be_req->glxCode = req->glxCode;
       be_req->length = req->length;
       be_req->contextTag = GetCurrentBackEndTag(cl,req->contextTag,s);
       if (buf_size > 0) 
	  _XSend(dpy, (const char *)pc, buf_size);
       UnlockDisplay(dpy);
       SyncHandle();

       if (req->glxCode == X_GLsop_Flush) {
	  XFlush(dpy);
       }

    }

    return Success;
}

int __glXForwardPipe0WithReply( __GLXclientState *cl, GLbyte *pc )
{
   ClientPtr client = cl->client;
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   xGLXSingleReq *be_req;
   xGLXSingleReply reply;
   xGLXSingleReply be_reply;
    __GLXcontext *glxc;
   int buf_size;
   char *be_buf = NULL;
   int   be_buf_size;
   DMXScreenInfo *dmxScreen;
   Display *dpy;

    glxc = __glXLookupContextByTag(cl, req->contextTag);
    if (!glxc) {
	return __glXBadContext;
    }

    pc += sz_xGLXSingleReq;
    buf_size = (req->length << 2) - sz_xGLXSingleReq;

    dmxScreen = &dmxScreens[glxc->pScreen->myNum];
    dpy = GetBackEndDisplay(cl, glxc->pScreen->myNum);

    /* 
     * send the request to the first back-end server
     */
    LockDisplay(dpy);
    GetReqSingle(GLXSingle,be_req);
    be_req->reqType = dmxScreen->glxMajorOpcode;
    be_req->glxCode = req->glxCode;
    be_req->length = req->length;
    be_req->contextTag = GetCurrentBackEndTag(cl,req->contextTag,glxc->pScreen->myNum);
    if (buf_size > 0) 
       _XSend(dpy, (const char *)pc, buf_size);

    /*
     * get the reply from the back-end server
     */
    _XReply(dpy, (xReply*) &be_reply, 0, False);
    be_buf_size = be_reply.length << 2;
    if (be_buf_size > 0) {
       be_buf = (char *)Xalloc( be_buf_size );
       if (be_buf) {
	  _XRead(dpy, be_buf, be_buf_size);
       }
       else {
	  /* Throw data on the floor */
	  _XEatData(dpy, be_buf_size);
	  return BadAlloc;
       }
    }

    UnlockDisplay(dpy);
    SyncHandle();

    /*
     * send the reply to the client
     */
    reply.type = X_Reply;
    reply.sequenceNumber = client->sequence;
    reply.length = be_reply.length;
    reply.retval = be_reply.retval;
    reply.size = be_reply.size;
    reply.pad3 = be_reply.pad3;
    reply.pad4 = be_reply.pad4;

    if (client->swapped) {
       SendSwappedReply( client, &reply, be_buf, be_buf_size );
    }
    else {
       WriteToClient(client, sizeof(xGLXSingleReply),(char *)&reply);
       if (be_buf_size > 0)
	  WriteToClient(client, be_buf_size, (char *)be_buf);
    }

    if (be_buf_size > 0) Xfree(be_buf);

    return Success;
}

int __glXForwardAllWithReply( __GLXclientState *cl, GLbyte *pc )
{
   ClientPtr client = cl->client;
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   xGLXSingleReq *be_req;
   xGLXSingleReply reply;
   xGLXSingleReply be_reply;
    __GLXcontext *glxc;
   int buf_size;
   char *be_buf = NULL;
   int   be_buf_size = 0;
   int from_screen = 0;
   int to_screen = 0;
   int s;

   DMXScreenInfo *dmxScreen;
   Display *dpy;

    glxc = __glXLookupContextByTag(cl, req->contextTag);
    if (!glxc) {
	return 0;
    }
    from_screen = to_screen = glxc->pScreen->myNum;

#ifdef PANORAMIX
    if (!noPanoramiXExtension) {
       from_screen = 0;
       to_screen = screenInfo.numScreens - 1;
    }
#endif

    pc += sz_xGLXSingleReq;
    buf_size = (req->length << 2) - sz_xGLXSingleReq;

    /* 
     * send the request to the first back-end server(s)
     */
    for (s=to_screen; s>=from_screen; s--) {
       dmxScreen = &dmxScreens[s];
       dpy = GetBackEndDisplay(cl,s);

       LockDisplay(dpy);
       GetReqSingle(GLXSingle,be_req);
       be_req->reqType = dmxScreen->glxMajorOpcode;
       be_req->glxCode = req->glxCode;
       be_req->length = req->length;
       be_req->contextTag = GetCurrentBackEndTag(cl,req->contextTag,s);
       if (buf_size > 0) 
	  _XSend(dpy, (const char *)pc, buf_size);

       /*
	* get the reply from the back-end server
	*/
       _XReply(dpy, (xReply*) &be_reply, 0, False);
       be_buf_size = be_reply.length << 2;
       if (be_buf_size > 0) {
	  be_buf = (char *)Xalloc( be_buf_size );
	  if (be_buf) {
	     _XRead(dpy, be_buf, be_buf_size);
	  }
	  else {
	     /* Throw data on the floor */
	     _XEatData(dpy, be_buf_size);
	     return BadAlloc;
	  }
       }

       UnlockDisplay(dpy);
       SyncHandle();

       if (s > from_screen && be_buf_size > 0) {
	  Xfree(be_buf);
       }
    }

    /*
     * send the reply to the client
     */
    reply.type = X_Reply;
    reply.sequenceNumber = client->sequence;
    reply.length = be_reply.length;
    reply.retval = be_reply.retval;
    reply.size = be_reply.size;
    reply.pad3 = be_reply.pad3;
    reply.pad4 = be_reply.pad4;

    if (client->swapped) {
       SendSwappedReply( client, &reply, be_buf, be_buf_size );
    }
    else {
       WriteToClient(client, sizeof(xGLXSingleReply),(char *)&reply);
       if (be_buf_size > 0)
	  WriteToClient(client, be_buf_size, (char *)be_buf);
    }

    if (be_buf_size > 0) Xfree(be_buf);

    return Success;
}

int __glXForwardSingleReqSwap( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 0;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }

   return( __glXForwardSingleReq( cl, pc ) );
}

int __glXForwardPipe0WithReplySwap( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 0;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }

   return( __glXForwardPipe0WithReply( cl, pc ) );
}

int __glXForwardPipe0WithReplySwapsv( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 2;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardPipe0WithReply( cl, pc ) );
}

int __glXForwardPipe0WithReplySwapiv( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 4;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardPipe0WithReply( cl, pc ) );
}

int __glXForwardPipe0WithReplySwapdv( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 8;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardPipe0WithReply( cl, pc ) );
}

int __glXForwardAllWithReplySwap( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 0;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardAllWithReply( cl, pc ) );
}

int __glXForwardAllWithReplySwapsv( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 2;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardAllWithReply( cl, pc ) );
}

int __glXForwardAllWithReplySwapiv( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 4;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardAllWithReply( cl, pc ) );
}

int __glXForwardAllWithReplySwapdv( __GLXclientState *cl, GLbyte *pc )
{
   xGLXSingleReq *req = (xGLXSingleReq *)pc;
   __GLX_DECLARE_SWAP_VARIABLES;
   __GLX_DECLARE_SWAP_ARRAY_VARIABLES;

   __GLX_SWAP_SHORT(&req->length);
   __GLX_SWAP_INT(&req->contextTag);

   swap_vec_element_size = 8;

   /*
    * swap extra data in request - assuming all data
    * (if available) are arrays of 4 bytes components !
    */
   if (req->length > sz_xGLXSingleReq/4) {
      int *data = (int *)(req+1);
      int count = req->length - sz_xGLXSingleReq/4;
      __GLX_SWAP_INT_ARRAY(data, count );
   }


   return( __glXForwardAllWithReply( cl, pc ) );
}

static GLint __glReadPixels_size(GLenum format, GLenum type, GLint w, GLint h, 
                          int *elementbits_return, int *rowbytes_return )
{
    GLint elements, esize;
    GLint rowsize, padding;

    if (w < 0 || h < 0) {
	return -1;
    }
    switch (format) {
      case GL_COLOR_INDEX:
      case GL_STENCIL_INDEX:
      case GL_DEPTH_COMPONENT:
	elements = 1;
	break;
      case GL_RED:
      case GL_GREEN:
      case GL_BLUE:
      case GL_ALPHA:
      case GL_LUMINANCE:
	elements = 1;
	break;
      case GL_LUMINANCE_ALPHA:
	elements = 2;
	break;
      case GL_RGB:
      case GL_BGR:
	elements = 3;
	break;
      case GL_RGBA:
      case GL_BGRA:
      case GL_ABGR_EXT:
	elements = 4;
	break;
      default:
	return -1;
    }
    /*
    ** According to the GLX protocol, each row must be padded to a multiple of
    ** 4 bytes.  4 bytes also happens to be the default alignment in the pixel
    ** store modes of the GL.
    */
    switch (type) {
      case GL_BITMAP:
	if (format == GL_COLOR_INDEX || format == GL_STENCIL_INDEX) {
	   rowsize = ((w * elements)+7)/8;
	   padding = rowsize % 4;
	   if (padding) {
	      rowsize += 4 - padding;
	   }
	   if (elementbits_return) *elementbits_return = elements;
	   if (rowbytes_return) *rowbytes_return = rowsize;
	   return (rowsize * h);
	} else {
	   return -1;
	}
      case GL_BYTE:
      case GL_UNSIGNED_BYTE:
	esize = 1;
	break;
      case GL_UNSIGNED_BYTE_3_3_2:
      case GL_UNSIGNED_BYTE_2_3_3_REV:
	esize = 1;
	elements = 1;
	break;
      case GL_SHORT:
      case GL_UNSIGNED_SHORT:
	esize = 2;
	break;
      case GL_UNSIGNED_SHORT_5_6_5:
      case GL_UNSIGNED_SHORT_5_6_5_REV:
      case GL_UNSIGNED_SHORT_4_4_4_4:
      case GL_UNSIGNED_SHORT_4_4_4_4_REV:
      case GL_UNSIGNED_SHORT_5_5_5_1:
      case GL_UNSIGNED_SHORT_1_5_5_5_REV:
	esize = 2;
	elements = 1;
	break;
      case GL_INT:
      case GL_UNSIGNED_INT:
      case GL_FLOAT:
	esize = 4;
	break;
      case GL_UNSIGNED_INT_8_8_8_8:
      case GL_UNSIGNED_INT_8_8_8_8_REV:
      case GL_UNSIGNED_INT_10_10_10_2:
      case GL_UNSIGNED_INT_2_10_10_10_REV:
	esize = 4;
	elements = 1;
	break;
      default:
	return -1;
    }
    rowsize = w * elements * esize;
    padding = rowsize % 4;
    if (padding) {
	rowsize += 4 - padding;
    }

    if (elementbits_return) *elementbits_return = esize*elements*8;
    if (rowbytes_return) *rowbytes_return = rowsize;

    return (rowsize * h);
}

static int intersectRect( int x1, int x2, int y1, int y2,
                    int X1, int X2, int Y1, int Y2,
		    int *ix1, int *ix2, int *iy1, int *iy2 )
{
   int right = (x2 < X2 ? x2 : X2);
   int bottom = (y2 < Y2 ? y2 : Y2);
   int left = (x1 > X1 ? x1 : X1);
   int top = (y1 > Y1 ? y1 : Y1);
   int width = right - left + 1;
   int height = bottom - top + 1;

   if ( (width <= 0) || (height <= 0) ) {
      *ix1 = *ix2 = *iy1 = *iy2 = 0;
      return(0);
   }
   else {
      *ix1 = left;
      *ix2 = right;
      *iy1 = top;
      *iy2 = bottom;
      return( width * height );
   }
}

int __glXDisp_ReadPixels(__GLXclientState *cl, GLbyte *pc)
{
    xGLXSingleReq *req = (xGLXSingleReq *)pc;
    xGLXSingleReq *be_req;
    xGLXReadPixelsReply reply;
    xGLXReadPixelsReply be_reply;
    GLbyte *be_pc;
    GLint x,y;
    GLsizei width, height;
    GLenum format, type;
    GLboolean swapBytes, lsbFirst;
    ClientPtr client = cl->client;
    DrawablePtr pDraw;
    __GLXcontext *glxc;
    int from_screen = 0;
    int to_screen = 0;
    char *buf = NULL;
    int buf_size;
    int s;
    int win_x1, win_x2;
    int win_y1, win_y2;
    int ebits, rowsize;
    __GLX_DECLARE_SWAP_VARIABLES;

    if (client->swapped) {
       __GLX_SWAP_INT(&req->contextTag);
    }

    glxc = __glXLookupContextByTag(cl, req->contextTag);
    if (!glxc) {
	return 0;
    }
    from_screen = to_screen = glxc->pScreen->myNum;

#ifdef PANORAMIX
    if (!noPanoramiXExtension) {
       from_screen = 0;
       to_screen = screenInfo.numScreens - 1;
    }
#endif

    pc += sz_xGLXSingleReq;
    x = *(GLint *)(pc + 0);
    y = *(GLint *)(pc + 4);
    width = *(GLsizei *)(pc + 8);
    height = *(GLsizei *)(pc + 12);
    format = *(GLenum *)(pc + 16);
    type = *(GLenum *)(pc + 20);
    swapBytes = *(GLboolean *)(pc + 24);
    lsbFirst = *(GLboolean *)(pc + 25);

    if (client->swapped) {
       __GLX_SWAP_INT(&x);
       __GLX_SWAP_INT(&y);
       __GLX_SWAP_INT(&width);
       __GLX_SWAP_INT(&height);
       __GLX_SWAP_INT(&format);
       __GLX_SWAP_INT(&type);
       swapBytes = !swapBytes;
    }

    buf_size = __glReadPixels_size(format,type,width,height, &ebits, &rowsize);
    if (buf_size >= 0) {
       buf = (char *) Xalloc( buf_size );
       if ( !buf ) {
	  return( BadAlloc );
       }
    }
    else {
       buf_size = 0;
    }

    if (buf_size > 0) {
       /*
	* Get the current drawable this context is bound to
	*/
       pDraw = __glXLookupDrawableByTag( cl, req->contextTag );
       win_x1 = pDraw->x + x;
       win_x2 = win_x1 + width - 1;
       win_y1 = (dmxGlobalHeight - pDraw->y - pDraw->height) + y;
       win_y2 = win_y1 + height - 1;
       if (pDraw->type != DRAWABLE_WINDOW) {
	  from_screen = to_screen = 0;
       }

       for (s=from_screen; s<=to_screen; s++) {
	  DMXScreenInfo *dmxScreen = &dmxScreens[s];
	  Display *dpy = GetBackEndDisplay(cl,s);
	  int scr_x1 = dmxScreen->rootXOrigin;
	  int scr_x2 = dmxScreen->rootXOrigin + dmxScreen->scrnWidth - 1;
	  int scr_y1 = dmxScreen->rootYOrigin;
	  int scr_y2 = dmxScreen->rootYOrigin + dmxScreen->scrnHeight - 1;
	  int wx1, wx2, wy1, wy2;
	  int sx, sy, sw, sh;
	  int npixels;

	  /*
	   * find the window portion that is on the current screen
	   */
	  if (pDraw->type == DRAWABLE_WINDOW) {
	     npixels = intersectRect( scr_x1, scr_x2, scr_y1, scr_y2,
		   win_x1, win_x2, win_y1, win_y2,
		   &wx1, &wx2, &wy1, &wy2 );
	  }
	  else {
	     wx1 = win_x1;
	     wx2 = win_x2;
	     wy1 = win_y1;
	     wy2 = win_y2;
	     npixels = (wx2-wx1+1) * (wy2-wy1+1);
	  }

   	  if (npixels > 0) {

   	     /* send the request to the back-end server */
   	     LockDisplay(dpy);
   	     GetReqExtra(GLXSingle,__GLX_PAD(26),be_req);
   	     be_req->reqType = dmxScreen->glxMajorOpcode;
   	     be_req->glxCode = X_GLsop_ReadPixels;
   	     be_req->contextTag = GetCurrentBackEndTag(cl,req->contextTag,s);
   	     be_pc = ((GLbyte *)(be_req) + sz_xGLXSingleReq);

   	     sx = wx1 - pDraw->x;
   	     sy = wy1 - (dmxGlobalHeight - pDraw->y - pDraw->height); 
   	     sw = (wx2-wx1+1); 
   	     sh = (wy2-wy1+1); 
	     
	     *(GLint *)(be_pc + 0) = sx;    /* x */
	     *(GLint *)(be_pc + 4) = sy;    /* y */
	     *(GLsizei *)(be_pc + 8) = sw;    /* width */
	     *(GLsizei *)(be_pc + 12) = sh;   /* height */
	     *(GLenum *)(be_pc + 16) = format; 
	     *(GLenum *)(be_pc + 20) = type;
	     *(GLboolean *)(be_pc + 24) = swapBytes;
	     *(GLboolean *)(be_pc + 25) = lsbFirst;

	     _XReply(dpy, (xReply*) &be_reply, 0, False);
         
	     if (be_reply.length > 0) {
		char *be_buf;
		int be_buf_size = be_reply.length << 2;

		be_buf = (char *) Xalloc( be_buf_size );
		if (be_buf) {
		   _XRead(dpy, be_buf, be_buf_size);

		   /* copy pixels data to the right location of the */
		   /* reply buffer */
		   if ( type != GL_BITMAP ) {
		      int pbytes = ebits / 8;
		      char *dst = buf + (sy-y)*rowsize + (sx-x)*pbytes;
		      char *src = be_buf;
		      int pad = (pbytes * sw) % 4;
		      int r;

		      for (r=0; r<sh; r++) {
			 memcpy( dst, src, pbytes*sw );
			 dst += rowsize;
			 src += (pbytes*sw + (pad ? 4-pad : 0) );
		      }
		   }
		   else {
		      /* this is a GL_BITMAP pixel type, should copy bits */
		      int r;
		      int src_rowsize = ((sw * ebits) + 7) / 8;
		      int src_pad = src_rowsize % 4;
                      if ( src_pad ) {
			 src_rowsize += (4 - src_pad);
		      }

		      for (r=0; r<sh; r++) {
			 unsigned char dst_mask = 0x80 >> (sx % 8);
			 unsigned char src_mask = 0x80;
			 char *dst = buf + (sy-y+r)*rowsize + (sx-x)/8;
			 char *src = be_buf + r*src_rowsize;
			 int b;

			 for (b=0; b<sw*ebits; b++) {
			    if ( *src & src_mask ) {
			       *dst |= dst_mask;
			    }
			    else {
			       *dst &= ~dst_mask;
			    }

			    if (dst_mask > 1) dst_mask >>= 1;
			    else { 
			       dst_mask = 0x80;
			       dst++;
			    }

			    if (src_mask > 1) src_mask >>= 1;
			    else {
			       src_mask = 0x80;
			       src++;
			    }
			 }
		      }

		   }

		   Xfree( be_buf );
		}
		else {
		   /* Throw data on the floor */
		   _XEatData(dpy, be_buf_size);
		   Xfree( buf );
		   return BadAlloc;
		}
	     }

	     UnlockDisplay(dpy);
	     SyncHandle();

	  } /* of npixels > 0 */ 

       }  /* of for loop */

    } /* of if buf_size > 0 */

    reply.type = X_Reply;
    reply.sequenceNumber = client->sequence;
    reply.length = buf_size >> 2;

    if (client->swapped) {
       __GLX_SWAP_SHORT(&reply.sequenceNumber);
       __GLX_SWAP_INT(&reply.length);
    }

    WriteToClient(client, sizeof(xGLXReadPixelsReply),(char *)&reply);
    if (buf_size > 0) {
       WriteToClient(client, buf_size, (char *)buf);
       Xfree( buf );
    }

    return Success;
}

int __glXDispSwap_GetTexImage(__GLXclientState *cl, GLbyte *pc)
{
    __GLX_DECLARE_SWAP_VARIABLES;
    GLbyte *lpc = pc;

    lpc += sz_xGLXSingleReq;
    __GLX_SWAP_INT(lpc+0);
    __GLX_SWAP_INT(lpc+4);
    __GLX_SWAP_INT(lpc+8);
    __GLX_SWAP_INT(lpc+12);

    /* reverse swapBytes */
    *(GLboolean *)(lpc + 16) = ! *(GLboolean *)(lpc + 16);

    return( __glXForwardPipe0WithReplySwap( cl, pc ) );
}

int __glXDispSwap_GetColorTable(__GLXclientState *cl, GLbyte *pc)
{
    __GLX_DECLARE_SWAP_VARIABLES;
    GLbyte *lpc = pc;

    lpc += sz_xGLXSingleReq;
    __GLX_SWAP_INT(lpc+0);
    __GLX_SWAP_INT(lpc+4);
    __GLX_SWAP_INT(lpc+8);

    /* reverse swapBytes */
    *(GLboolean *)(lpc + 12) = ! *(GLboolean *)(lpc + 12);

    return( __glXForwardPipe0WithReplySwap( cl, pc ) );
}


