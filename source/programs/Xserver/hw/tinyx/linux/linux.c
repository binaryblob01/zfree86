/*
 * $XFree86: xc/programs/Xserver/hw/tinyx/linux/linux.c,v 1.3 2005/10/14 15:16:28 tsi Exp $
 *
 * Copyright � 1999 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2004 by The XFree86 Project, Inc.
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

#include "tinyx.h"
#include "linux.h"
#include <errno.h>
#include <signal.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <X11/keysym.h>
#include <linux/apm_bios.h>

static int  vtno;
int  LinuxConsoleFd;
int  LinuxApmFd = -1;
static int  activeVT;
static Bool enabled;

static void
LinuxVTRequest (int sig)
{
    kdSwitchPending = TRUE;
}

/* Check before chowning -- this avoids touching the file system */
static void
LinuxCheckChown (char *file)
{
    struct stat	    st;
    __uid_t	    u;
    __gid_t	    g;

    if (stat (file, &st) < 0)
	return;
    u = getuid ();
    g = getgid ();
    if (st.st_uid != u || st.st_gid != g)
	chown (file, u, g);
}

static int
LinuxInit (void)
{
    int fd;
    char vtname[11];
    struct vt_stat vts;

    LinuxConsoleFd = -1;
    /* check if we're run with euid==0 */
    if (geteuid() != 0)
    {
	FatalError("LinuxInit: Server must be suid root\n");
    }

    if (kdVirtualTerminal >= 0)
	vtno = kdVirtualTerminal;
    else
    {
	if ((fd = open("/dev/tty0",O_WRONLY,0)) < 0) 
	{
	    FatalError(
		       "LinuxInit: Cannot open /dev/tty0 (%s)\n",
		       strerror(errno));
	}
	if ((ioctl(fd, VT_OPENQRY, &vtno) < 0) ||
	    (vtno == -1))
	{
	    FatalError("xf86OpenConsole: Cannot find a free VT\n");
	}
	close(fd);
    }

    sprintf(vtname,"/dev/tty%d",vtno); /* /dev/tty1-64 */

    if ((LinuxConsoleFd = open(vtname, O_RDWR|O_NDELAY, 0)) < 0)
    {
	FatalError("LinuxInit: Cannot open %s (%s)\n",
		   vtname, strerror(errno));
    }

    /* change ownership of the vt */
    LinuxCheckChown (vtname);

    /*
     * the current VT device we're running on is not "console", we want
     * to grab all consoles too
     *
     * Why is this needed?
     */
    LinuxCheckChown ("/dev/tty0");
    /*
     * Linux doesn't switch to an active vt after the last close of a vt,
     * so we do this ourselves by remembering which is active now.
     */
    if (ioctl(LinuxConsoleFd, VT_GETSTATE, &vts) == 0)
    {
	activeVT = vts.v_active;
    }

    return 1;
}

Bool
LinuxFindPci (CARD16 vendor, CARD16 device, CARD32 count, KdCardAttr *attr)
{
    FILE    *f;
    char    line[2048], *l, *end;
    CARD32  bus, id, addr;
    int	    n;
    CARD32  ven_dev;
    Bool    ret = FALSE;
    int	    i;

    ven_dev = (((CARD32) vendor) << 16) | ((CARD32) device);
    f = fopen ("/proc/bus/pci/devices", "r");
    if (!f)
	return FALSE;
    attr->io = 0;
    while (fgets (line, sizeof (line)-1, f))
    {
	line[sizeof(line)-1] = '\0';
	l = line;
	bus = strtoul (l, &end, 16);
	if (end == l)
	    continue;
	l = end;
	id = strtoul (l, &end, 16);
	if (end == l)
	    continue;
	l = end;
	if (id != ven_dev)
	    continue;
	if (count--)
	    continue;
	(void) strtoul (l, &end, 16);
	if (end == l)
	    continue;
	l = end;
	n = 0;
	for (i = 0; i < 6; i++)
	{
	    addr = strtoul (l, &end, 16);
	    if (end == l)
		break;
	    if (addr & 1)
		attr->io = addr & ~0xf;
	    else
	    {
		if (n == KD_MAX_CARD_ADDRESS)
		    break;
		attr->address[n++] = addr & ~0xf;
	    }
	    l = end;
	}
	while (n > 0)
	{
	    if (attr->address[n-1] != 0)
		break;
	    n--;
	}
	attr->naddr = n;
        attr->bus = bus;
	ret = TRUE;
	break;
    }
    fclose (f);
    return ret;
}

unsigned char *
LinuxGetPciCfg(KdCardAttr *attr) {
    char filename[256];
    FILE *f;
    unsigned char *cfg;
    int r;

    snprintf(filename, 255, "/proc/bus/pci/%02x/%02x.%x",
             attr->bus >> 8, (attr->bus & 0xff) >> 3, attr->bus & 7);
/*     fprintf(stderr,"Find card on path %s\n",filename); */

    if (!(f=fopen(filename,"r"))) 
        return NULL;

    if (!(cfg=xalloc(256))) 
    {
        fclose(f);
        return NULL;
    }

    if (256 != (r=fread(cfg, 1, 256, f)))
    {
        fprintf(stderr,"LinuxGetPciCfg: read %d, expected 256\n",r);
        free(cfg);
        cfg=NULL;
    }
    fclose(f);
/*     fprintf(stderr,"LinuxGetPciCfg: success, returning %p\n",cfg); */
    return cfg;
}

static void
LinuxSetSwitchMode (int mode)
{
    struct sigaction	act;
    struct vt_mode	VT;
    
    if (ioctl(LinuxConsoleFd, VT_GETMODE, &VT) < 0) 
    {
	FatalError ("LinuxInit: VT_GETMODE failed\n");
    }

    (void) memset(&act, 0, sizeof(act));

    if (mode == VT_PROCESS)
    {
	act.sa_handler = LinuxVTRequest;
	sigaction (SIGUSR1, &act, 0);
    
	VT.mode = mode;
	VT.relsig = SIGUSR1;
	VT.acqsig = SIGUSR1;
    }
    else
    {
	act.sa_handler = SIG_IGN;
	sigaction (SIGUSR1, &act, 0);
    
	VT.mode = mode;
	VT.relsig = 0;
	VT.acqsig = 0;
    }
    if (ioctl(LinuxConsoleFd, VT_SETMODE, &VT) < 0) 
    {
	FatalError("LinuxInit: VT_SETMODE failed\n");
    }
}

static void
LinuxApmBlock (pointer blockData, OSTimePtr pTimeout, pointer pReadmask)
{
}

static Bool LinuxApmRunning;

static void
LinuxApmWakeup (pointer blockData, int result, pointer pReadmask)
{
    fd_set  *readmask = (fd_set *) pReadmask;

    if (result > 0 && LinuxApmFd >= 0 && FD_ISSET (LinuxApmFd, readmask))
    {
	apm_event_t event;
	Bool	    running = LinuxApmRunning;
	int	    cmd = APM_IOC_SUSPEND;

	while (read (LinuxApmFd, &event, sizeof (event)) == sizeof (event))
	{
	    switch (event) {
	    case APM_SYS_STANDBY:
	    case APM_USER_STANDBY:
		running = FALSE;
		cmd = APM_IOC_STANDBY;
		break;
	    case APM_SYS_SUSPEND:
	    case APM_USER_SUSPEND:
	    case APM_CRITICAL_SUSPEND:
		running = FALSE;
		cmd = APM_IOC_SUSPEND;
		break;
	    case APM_NORMAL_RESUME:
	    case APM_CRITICAL_RESUME:
	    case APM_STANDBY_RESUME:
		running = TRUE;
		break;
	    }
	}
	if (running && !LinuxApmRunning)
	{
	    KdResume ();
	    LinuxApmRunning = TRUE;
	}
	else if (!running && LinuxApmRunning)
	{
	    KdSuspend ();
	    LinuxApmRunning = FALSE;
	    ioctl (LinuxApmFd, cmd, 0);
	}
    }
}

#ifdef FNONBLOCK
#define NOBLOCK FNONBLOCK
#else
#define NOBLOCK FNDELAY
#endif

static void
LinuxEnable (void)
{
    if (enabled)
	return;
    if (kdSwitchPending)
    {
	kdSwitchPending = FALSE;
	ioctl (LinuxConsoleFd, VT_RELDISP, VT_ACKACQ);
    }
    /*
     * Open the APM driver
     */
    LinuxApmFd = open ("/dev/apm_bios", 2);
    if (LinuxApmFd >= 0)
    {
	LinuxApmRunning = TRUE;
	fcntl (LinuxApmFd, F_SETFL, fcntl (LinuxApmFd, F_GETFL) | NOBLOCK);
	RegisterBlockAndWakeupHandlers (LinuxApmBlock, LinuxApmWakeup, 0);
	AddEnabledDevice (LinuxApmFd);
    }
	
    /*
     * now get the VT
     */
    LinuxSetSwitchMode (VT_AUTO);
    if (ioctl(LinuxConsoleFd, VT_ACTIVATE, vtno) != 0)
    {
	FatalError("LinuxInit: VT_ACTIVATE failed\n");
    }
    if (ioctl(LinuxConsoleFd, VT_WAITACTIVE, vtno) != 0)
    {
	FatalError("LinuxInit: VT_WAITACTIVE failed\n");
    }
    LinuxSetSwitchMode (VT_PROCESS);
    if (ioctl(LinuxConsoleFd, KDSETMODE, KD_GRAPHICS) < 0)
    {
	FatalError("LinuxInit: KDSETMODE KD_GRAPHICS failed\n");
    }
    enabled = TRUE;
}

static Bool
LinuxSpecialKey (KeySym sym)
{
    struct vt_stat  vts;
    int		    con;
    
    if (XK_F1 <= sym && sym <= XK_F12)
    {
	con = sym - XK_F1 + 1;
	ioctl (LinuxConsoleFd, VT_GETSTATE, &vts);
	if (con != vts.v_active && (vts.v_state & (1 << con)))
	{
	    ioctl (LinuxConsoleFd, VT_ACTIVATE, con);
	    return TRUE;
	}
    }
    return FALSE;
}

static void
LinuxDisable (void)
{
    ioctl(LinuxConsoleFd, KDSETMODE, KD_TEXT);  /* Back to text mode ... */
    if (kdSwitchPending)
    {
	kdSwitchPending = FALSE;
	ioctl (LinuxConsoleFd, VT_RELDISP, 1);
    }
    enabled = FALSE;
    if (LinuxApmFd >= 0)
    {
	RemoveBlockAndWakeupHandlers (LinuxApmBlock, LinuxApmWakeup, 0);
	RemoveEnabledDevice (LinuxApmFd);
	close (LinuxApmFd);
	LinuxApmFd = -1;
    }
}

static void
LinuxFini (void)
{
    struct vt_mode   VT;
    struct vt_stat  vts;
    int		    fd;

    if (LinuxConsoleFd < 0)
	return;

    if (ioctl(LinuxConsoleFd, VT_GETMODE, &VT) != -1)
    {
	VT.mode = VT_AUTO;
	ioctl(LinuxConsoleFd, VT_SETMODE, &VT); /* set dflt vt handling */
    }
    ioctl (LinuxConsoleFd, VT_GETSTATE, &vts);
    /*
     * Find a legal VT to switch to, either the one we started from
     * or the lowest active one that isn't ours
     */
    if (activeVT < 0 || 
	activeVT == vts.v_active || 
	!(vts.v_state & (1 << activeVT)))
    {
	for (activeVT = 1; activeVT < 16; activeVT++)
	    if (activeVT != vtno && (vts.v_state & (1 << activeVT)))
		break;
	if (activeVT == 16)
	    activeVT = -1;
    }
    /*
     * Perform a switch back to the active VT when we were started
     */
    if (activeVT >= -1)
    {
	ioctl (LinuxConsoleFd, VT_ACTIVATE, activeVT);
	ioctl (LinuxConsoleFd, VT_WAITACTIVE, activeVT);
	activeVT = -1;
    }
    close(LinuxConsoleFd);                /* make the vt-manager happy */
    fd = open ("/dev/tty0", O_RDWR|O_NDELAY, 0);
    if (fd >= 0)
    {
	ioctl (fd, VT_GETSTATE, &vts);
	if (ioctl (fd, VT_DISALLOCATE, vtno) < 0)
	    fprintf (stderr, "Can't deallocate console %d errno %d\n", vtno, errno);
	close (fd);
    }
    return;
}

KdOsFuncs   LinuxFuncs = {
    LinuxInit,
    LinuxEnable,
    LinuxSpecialKey,
    LinuxDisable,
    LinuxFini,
};

void
OsVendorPreInit (void)
{
}

void
OsVendorInit (void)
{
    KdOsInit (&LinuxFuncs);
}
