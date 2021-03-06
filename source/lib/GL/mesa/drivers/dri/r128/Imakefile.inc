XCOMM $XFree86: xc/lib/GL/mesa/drivers/dri/r128/Imakefile.inc,v 1.6 2005/10/14 15:15:57 tsi Exp $

#ifndef MesaDrvSrcDir
#define MesaDrvSrcDir $(MESASRCDIR)/src/mesa/drivers/dri
#endif

MESADRVSRCDIR = MesaDrvSrcDir

#ifndef MesaDrvR128BuildDir
#define MesaDrvR128BuildDir /**/
#endif
MESADRVR128BUILDDIR = MesaDrvR128BuildDir

#if Malloc0ReturnsNull
ALLOC_DEFINES = -DMALLOC_0_RETURNS_NULL
#endif

#if BuildXF86DRI
  DRI_DEFINES = GlxDefines -DX_BYTE_ORDER=$(X_BYTE_ORDER)
 DRI_INCLUDES = -I$(GLXLIBSRC)/dri \
		-I$(GLXLIBSRC)/glx \
		-I$(GLXLIBSRC)/include \
		-I$(SERVERSRC)/GL/dri \
		-I$(XF86OSSRC) \
		-I$(XF86COMSRC) \
		-I$(DRMINCLUDESDIR) \
		-I$(XF86DRIVERSRC)/ati \
		-I$(GLXLIBSRC)/dri/drm
#endif

MESA_INCLUDES = -I$(MESASRCDIR)/src/mesa \
		-I$(MESASRCDIR)/src/mesa/main \
		-I$(MESASRCDIR)/src/mesa/glapi \
		-I$(MESASRCDIR)/src/mesa/shader \
		-I$(MESASRCDIR)/include \
		-I$(MESADRVSRCDIR)/common \
		-I$(MESADRVSRCDIR)/r128

     R128SRCS = $(MESADRVR128BUILDDIR)r128_context.c \
		$(MESADRVR128BUILDDIR)r128_dd.c \
		$(MESADRVR128BUILDDIR)r128_ioctl.c \
		$(MESADRVR128BUILDDIR)r128_lock.c \
                $(MESADRVR128BUILDDIR)r128_screen.c \
		$(MESADRVR128BUILDDIR)r128_span.c \
		$(MESADRVR128BUILDDIR)r128_state.c \
		$(MESADRVR128BUILDDIR)r128_tex.c \
		$(MESADRVR128BUILDDIR)r128_texmem.c \
		$(MESADRVR128BUILDDIR)r128_texstate.c \
		$(MESADRVR128BUILDDIR)r128_tris.c

     R128OBJS = $(MESADRVR128BUILDDIR)r128_context.o \
		$(MESADRVR128BUILDDIR)r128_dd.o \
		$(MESADRVR128BUILDDIR)r128_ioctl.o \
		$(MESADRVR128BUILDDIR)r128_lock.o \
                $(MESADRVR128BUILDDIR)r128_screen.o \
		$(MESADRVR128BUILDDIR)r128_span.o \
		$(MESADRVR128BUILDDIR)r128_state.o \
		$(MESADRVR128BUILDDIR)r128_tex.o \
		$(MESADRVR128BUILDDIR)r128_texmem.o \
		$(MESADRVR128BUILDDIR)r128_texstate.o \
		$(MESADRVR128BUILDDIR)r128_tris.o

     R128EOBJS = $(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_context.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_dd.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_ioctl.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_lock.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_screen.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_span.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_state.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_tex.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_texmem.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_texstate.o \
		$(MESADRVR128BUILDDIR)$(EXTRASUBDIR)/r128_tris.o

     R128UOBJS = $(MESADRVR128BUILDDIR)unshared/r128_context.o \
		$(MESADRVR128BUILDDIR)unshared/r128_dd.o \
		$(MESADRVR128BUILDDIR)unshared/r128_ioctl.o \
		$(MESADRVR128BUILDDIR)unshared/r128_lock.o \
                $(MESADRVR128BUILDDIR)unshared/r128_screen.o \
		$(MESADRVR128BUILDDIR)unshared/r128_span.o \
		$(MESADRVR128BUILDDIR)unshared/r128_state.o \
		$(MESADRVR128BUILDDIR)unshared/r128_tex.o \
		$(MESADRVR128BUILDDIR)unshared/r128_texmem.o \
		$(MESADRVR128BUILDDIR)unshared/r128_texstate.o \
		$(MESADRVR128BUILDDIR)unshared/r128_tris.o

     R128DOBJS = $(MESADRVR128BUILDDIR)debugger/r128_context.o \
		$(MESADRVR128BUILDDIR)debugger/r128_dd.o \
		$(MESADRVR128BUILDDIR)debugger/r128_ioctl.o \
		$(MESADRVR128BUILDDIR)debugger/r128_lock.o \
                $(MESADRVR128BUILDDIR)debugger/r128_screen.o \
		$(MESADRVR128BUILDDIR)debugger/r128_span.o \
		$(MESADRVR128BUILDDIR)debugger/r128_state.o \
		$(MESADRVR128BUILDDIR)debugger/r128_tex.o \
		$(MESADRVR128BUILDDIR)debugger/r128_texmem.o \
		$(MESADRVR128BUILDDIR)debugger/r128_texstate.o \
		$(MESADRVR128BUILDDIR)debugger/r128_tris.o

     R128POBJS = $(MESADRVR128BUILDDIR)profiled/r128_context.o \
		$(MESADRVR128BUILDDIR)profiled/r128_dd.o \
		$(MESADRVR128BUILDDIR)profiled/r128_ioctl.o \
		$(MESADRVR128BUILDDIR)profiled/r128_lock.o \
                $(MESADRVR128BUILDDIR)profiled/r128_screen.o \
		$(MESADRVR128BUILDDIR)profiled/r128_span.o \
		$(MESADRVR128BUILDDIR)profiled/r128_state.o \
		$(MESADRVR128BUILDDIR)profiled/r128_tex.o \
		$(MESADRVR128BUILDDIR)profiled/r128_texmem.o \
		$(MESADRVR128BUILDDIR)profiled/r128_texstate.o \
		$(MESADRVR128BUILDDIR)profiled/r128_tris.o

#ifdef NeedToLinkMesaSrc
LinkSourceFile(r128_context.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_context.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_dd.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_dd.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_ioctl.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_ioctl.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_lock.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_lock.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_screen.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_screen.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_span.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_span.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_state.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_state.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_tex.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_tex.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_texmem.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_texobj.h, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_texstate.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_tris.c, $(MESADRVSRCDIR)/r128)
LinkSourceFile(r128_tris.h, $(MESADRVSRCDIR)/r128)
#endif
