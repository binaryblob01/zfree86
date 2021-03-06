/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86pciBus.c,v 3.96 2008/05/01 17:11:34 tsi Exp $ */
/*
 * Copyright (c) 1997-2007 by The XFree86 Project, Inc.
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
 * This file contains the interfaces to the bus-specific code
 */
#define INCLUDE_DEPRECATED 1

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/X.h>
#include "os.h"
#include "Pci.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Resources.h"

/* Bus-specific headers */
#include "xf86PciData.h"

#include "xf86Bus.h"

#define XF86_OS_PRIVS
#define NEED_OS_RAC_PROTOS
#include "xf86_OSproc.h"

#include "xf86RAC.h"

/* Bus-specific globals */
Bool pciSlotClaimed = FALSE;
pciConfigPtr *xf86PciInfo = NULL;		/* Full PCI probe info */
pciVideoPtr *xf86PciVideoInfo = NULL;		/* PCI probe for video hw */
pciAccPtr * xf86PciAccInfo = NULL;              /* PCI access related */

/* pcidata globals */
ScanPciSetupProcPtr xf86SetupPciIds = NULL;
ScanPciCloseProcPtr xf86ClosePciIds = NULL;
ScanPciFindByDeviceProcPtr xf86FindPciNamesByDevice = NULL;
ScanPciFindBySubsysProcPtr xf86FindPciNamesBySubsys = NULL;
ScanPciFindClassBySubsysProcPtr xf86FindPciClassBySubsys = NULL;
ScanPciFindClassByDeviceProcPtr xf86FindPciClassByDevice = NULL;

static resPtr pciAvoidRes = NULL;

/* PCI buses */
static PciBusPtr xf86PciBus = NULL;
/* Bus-specific probe/sorting functions */

/* PCI classes that get included in xf86PciVideoInfo */
#define PCIINFOCLASSES(b,s)						      \
    (((b) == PCI_CLASS_PREHISTORIC) ||					      \
     ((b) == PCI_CLASS_DISPLAY) ||					      \
     ((b) == PCI_CLASS_MULTIMEDIA && (s) == PCI_SUBCLASS_MULTIMEDIA_VIDEO) || \
     ((b) == PCI_CLASS_PROCESSOR && (s) == PCI_SUBCLASS_PROCESSOR_COPROC))

/*
 * PCI classes that have messages printed always.  The others are only
 * have a message printed when the vendor/dev IDs are recognised.
 */
#define PCIALWAYSPRINTCLASSES(b,s)					      \
    (((b) == PCI_CLASS_PREHISTORIC && (s) == PCI_SUBCLASS_PREHISTORIC_VGA) || \
     ((b) == PCI_CLASS_DISPLAY) ||					      \
     ((b) == PCI_CLASS_MULTIMEDIA && (s) == PCI_SUBCLASS_MULTIMEDIA_VIDEO))

/*
 * PCI classes for which potentially destructive checking of the map sizes
 * may be done.  Any classes where this may be unsafe should be omitted
 * from this list.
 */
#define PCINONSYSTEMCLASSES(b,s) PCIALWAYSPRINTCLASSES(b,s)

/*
 * PCI classes that use RAC
 */
#define PCISHAREDIOCLASSES(b,s)					      \
    (((b) == PCI_CLASS_PREHISTORIC && (s) == PCI_SUBCLASS_PREHISTORIC_VGA) || \
     ((b) == PCI_CLASS_DISPLAY && (s) == PCI_SUBCLASS_DISPLAY_VGA))

#define PCI_MEM32_LENGTH_MAX 0xFFFFFFFF

#undef MIN
#define MIN(x,y) ((x<y)?x:y)

#define B2M(tag,base) pciBusAddrToHostAddr(tag,PCI_MEM,base)
#define B2I(tag,base) pciBusAddrToHostAddr(tag,PCI_IO,base)
#define B2H(tag,base,type) (((type & ResPhysMask) == ResMem) ? \
			B2M(tag, base) : B2I(tag, base))
#define M2B(tag,base) pciHostAddrToBusAddr(tag,PCI_MEM,base)
#define I2B(tag,base) pciHostAddrToBusAddr(tag,PCI_IO,base)
#define H2B(tag,base,type) (((type & ResPhysMask) == ResMem) ? \
			M2B(tag, base) : I2B(tag, base))

#define B2ISB(tag, base) pciBusAddrToHostAddr(tag, PCI_IO_SPARSE_BASE, base)
#define B2ISM(tag, mask) pciBusAddrToHostAddr(tag, PCI_IO_SPARSE_MASK, mask)

#define TAG(pvp) (pciTag(pvp->bus,pvp->device,pvp->func))
#define SIZE(size) ((1 << size) - 1)
#define PCI_SIZE(type,tag,size) (((type & ResPhysMask) == ResMem) \
			? pciBusAddrToHostAddr(tag,PCI_MEM_SIZE,size) \
			: pciBusAddrToHostAddr(tag,PCI_IO_SIZE,size))
#define PCI_M_RANGE(range,tag,begin,end,type) \
	{ \
	    RANGE(range, B2M(tag, begin), B2M(tag, end), \
		  RANGE_TYPE(type, xf86GetPciDomain(tag))); \
	}
#define PCI_I_RANGE(range,tag,begin,end,type) \
	{ \
	    RANGE(range, B2I(tag, begin), B2I(tag, end), \
		  RANGE_TYPE(type, xf86GetPciDomain(tag))); \
	}
#define PCI_X_RANGE(range,tag,begin,end,type) \
{ if ((type & ResPhysMask) == ResMem)  PCI_M_RANGE(range,tag,begin,end,type); \
		else PCI_I_RANGE(range,tag,begin,end,type); }
#define P_M_RANGE(range,tag,begin,size,type) \
		    PCI_M_RANGE(range,tag,begin,(begin + SIZE(size)),type)
#define P_I_RANGE(range,tag,begin,size,type) \
		    PCI_I_RANGE(range,tag,begin,(begin + SIZE(size)),type)
#define P_X_RANGE(range,tag,begin,size,type) \
{ if ((type & ResPhysMask) == ResMem)  P_M_RANGE(range,tag,begin,size,type); \
		else P_I_RANGE(range,tag,begin,size,type); }
#define PV_M_RANGE(range,pvp,i,type) \
		  P_M_RANGE(range,TAG(pvp),pvp->memBase[i],pvp->size[i],type)
#define PV_B_RANGE(range,pvp,type) \
		  P_M_RANGE(range,TAG(pvp),pvp->biosBase,pvp->biosSize,type)
#define PV_I_RANGE(range,pvp,i,type) \
		  P_I_RANGE(range,TAG(pvp),pvp->ioBase[i],pvp->size[i],type)

static void getPciClassFlags(pciConfigPtr *pcrpp);
static void pciConvertListToHost(int bus, int dev, int func, resPtr list);
static PciBusPtr xf86GetPciBridgeInfo(void);
static int FindDiscardedPciSlot(int bus, int device, int func);

void
xf86FormatPciBusNumber(int busnum, char *buffer)
{
    /* 'buffer' should be at least 8 characters long */
    if (busnum < 256)
	sprintf(buffer, "%d", busnum);
    else
	sprintf(buffer, "%d@%d", busnum & 0x00ff, busnum >> 8);
}

static void
FindPCIVideoInfo(void)
{
    pciConfigPtr pcrp, *pcrpp;
    int i = 0, j, k;
    int num = 0;
    pciVideoPtr info;

    pcrpp = xf86PciInfo = xf86scanpci(0);
    getPciClassFlags(pcrpp);

    if (pcrpp == NULL) {
	xf86PciVideoInfo = NULL;
	return;
    }
    xf86PciBus = xf86GetPciBridgeInfo();

    while ((pcrp = pcrpp[i])) {
	int baseclass;
	int subclass;

	if (pcrp->listed_class & 0xffff) {
	    baseclass = (pcrp->listed_class >> 8) & 0xff;
	    subclass = pcrp->listed_class & 0xff;
	} else {
	    baseclass = pcrp->pci_base_class;
	    subclass = pcrp->pci_sub_class;
	}

	if (PCIINFOCLASSES(baseclass, subclass)) {
	    num++;
	    xf86PciVideoInfo = xnfrealloc(xf86PciVideoInfo,
					  sizeof(pciVideoPtr) * (num + 1));
	    xf86PciVideoInfo[num] = NULL;
	    info = xf86PciVideoInfo[num - 1] = xnfalloc(sizeof(pciVideoRec));
	    info->vendor = pcrp->pci_vendor;
	    info->chipType = pcrp->pci_device;
	    info->chipRev = pcrp->pci_rev_id;
	    info->subsysVendor = pcrp->pci_subsys_vendor;
	    info->subsysCard = pcrp->pci_subsys_card;
	    info->bus = pcrp->busnum;
	    info->device = pcrp->devnum;
	    info->func = pcrp->funcnum;
	    info->class = baseclass;
	    info->subclass = pcrp->pci_sub_class;
	    info->interface = pcrp->pci_prog_if;
	    info->biosBase = PCIGETROM(pcrp->pci_baserom);
	    info->biosSize = pciGetBaseSize(pcrp, 6, TRUE, &pcrp->minBasesize);
	    info->validSize = pcrp->minBasesize;
	    info->thisCard = pcrp;
	    info->validate = FALSE;
#ifdef INCLUDE_XF86_NO_DOMAIN
	    if ((PCISHAREDIOCLASSES(baseclass, subclass))
		&& (pcrp->pci_command & PCI_CMD_IO_ENABLE) &&
		(pcrp->pci_prog_if == 0)) {

		/*
		 * Attempt to ensure that VGA is actually routed to this
		 * adapter on entry.  This needs to be fixed when we finally
		 * grok host bridges (and multiple bus trees).
		 */
		j = info->bus;
		while (TRUE) {
		    PciBusPtr pBus = xf86PciBus;
		    while (pBus && j != pBus->secondary)
			pBus = pBus->next;
		    if (!pBus || !(pBus->brcontrol & PCI_PCI_BRIDGE_VGA_EN))
			break;
		    if (j == pBus->primary) {
			if (primaryBus.type == BUS_NONE) {
			    /* assumption: primary adapter is always VGA */
			    primaryBus.type = BUS_PCI;
			    primaryBus.id.pci.bus = pcrp->busnum;
			    primaryBus.id.pci.device = pcrp->devnum;
			    primaryBus.id.pci.func = pcrp->funcnum;
			} else if (primaryBus.type < BUS_last) {
			    xf86Msg(X_NOTICE,
				    "More than one primary device found\n");
			    primaryBus.type ^= (BusType)(-1);
			}
			break;
		    }
		    j = pBus->primary;
		}
	    }
#endif

	    for (j = 0; j < 6; j++) {
		info->memBase[j] = 0;
		info->ioBase[j] = 0;
		if (PCINONSYSTEMCLASSES(baseclass, subclass)) {
		    info->size[j] =
			pciGetBaseSize(pcrp, j, TRUE, &info->validSize);
		    pcrp->minBasesize = info->validSize;
		} else {
		    info->size[j] = pcrp->basesize[j];
		    info->validSize = pcrp->minBasesize;
		}
		info->type[j] = 0;
	    }

	    /*
	     * 64-bit base addresses are checked for and avoided on 32-bit
	     * platforms.
	     */
	    for (j = 0; j < 6; j++) {
		CARD32 bar = (&pcrp->pci_base0)[j];

		if (bar == 0)
		    continue;

		if (bar & PCI_MAP_IO) {
		    info->ioBase[j] = (memType)PCIGETIO(bar);
		    info->type[j] = bar & PCI_MAP_IO_ATTR_MASK;
		    continue;
		}

		info->type[j] = bar & PCI_MAP_MEMORY_ATTR_MASK;
		info->memBase[j] = (memType)PCIGETMEMORY(bar);
		if (!PCI_MAP_IS64BITMEM(bar))
		    continue;

		if (j == 5) {
		    xf86MsgVerb(X_WARNING, 0,
			"Ignoring 64-bit BAR 5 for PCI device %d:%d:%d\n",
			info->bus, info->device, info->func);
		    info->memBase[j] = 0;
		    break;
		}

		bar = (&pcrp->pci_base1)[j];
#if defined(LONG64) || defined(WORD64)
		info->memBase[j] |= (memType)bar << 32;
#else
		if (bar != 0)
		    info->memBase[j] = 0;
#endif
		j++;	/* Step over the next BAR */
	    }
	    info->listed_class = pcrp->listed_class;
	}
	i++;
    }

    /* If we haven't found a primary device try a different heuristic */
    if (primaryBus.type == BUS_NONE && num) {
	for (i = 0;  i < num;  i++) {
	    info = xf86PciVideoInfo[i];
	    pcrp = info->thisCard;

	    if ((pcrp->pci_command & PCI_CMD_MEM_ENABLE) &&
		(num == 1 ||
		 ((info->class == PCI_CLASS_DISPLAY) &&
		  (info->subclass == PCI_SUBCLASS_DISPLAY_MISC)))) {
		if (primaryBus.type == BUS_NONE) {
		    primaryBus.type = BUS_PCI;
		    primaryBus.id.pci.bus = pcrp->busnum;
		    primaryBus.id.pci.device = pcrp->devnum;
		    primaryBus.id.pci.func = pcrp->funcnum;
		} else {
		    xf86Msg(X_NOTICE,
			    "More than one possible primary device found\n");
		    primaryBus.type ^= (BusType)(-1);
		}
	    }
	}
    }

    /* Print a summary of the video devices found */
    for (k = 0; k < num; k++) {
	const char *vendorname = NULL, *chipname = NULL;
	const char *prim = " ";
	char busnum[8];
	Bool memdone = FALSE, iodone = FALSE;

	i = 0;
	info = xf86PciVideoInfo[k];
	xf86FormatPciBusNumber(info->bus, busnum);
	xf86FindPciNamesByDevice(info->vendor, info->chipType,
				 NOVENDOR, NOSUBSYS,
				 &vendorname, &chipname, NULL, NULL);
	if ((!vendorname || !chipname) &&
	    !PCIALWAYSPRINTCLASSES(info->class, info->subclass))
	    continue;
	if (xf86IsPrimaryPci(info))
	    prim = "*";

	xf86Msg(X_PROBED, "PCI:%s(%s:%d:%d) ", prim, busnum, info->device,
		info->func);
	if (vendorname)
	    xf86ErrorF("%s ", vendorname);
	else
	    xf86ErrorF("unknown vendor (0x%04x) ", info->vendor);
	if (chipname)
	    xf86ErrorF("%s ", chipname);
	else
	    xf86ErrorF("unknown chipset (0x%04x) ", info->chipType);
	xf86ErrorF("rev %d", info->chipRev);
	for (i = 0; i < 6; i++) {
	    if (info->memBase[i] &&
		(info->memBase[i] < (memType)(-1 << info->size[i]))) {
		if (!memdone) {
		    xf86ErrorF(", Mem @ ");
		    memdone = TRUE;
		} else
		    xf86ErrorF(", ");
		xf86ErrorF("0x%08lx/%d", info->memBase[i], info->size[i]);
	    }
	}
	for (i = 0; i < 6; i++) {
	    if (info->ioBase[i] &&
		(info->ioBase[i] < (memType)(-1 << info->size[i]))) {
		if (!iodone) {
		    xf86ErrorF(", I/O @ ");
		    iodone = TRUE;
		} else
		    xf86ErrorF(", ");
		xf86ErrorF("0x%04lx/%d", info->ioBase[i], info->size[i]);
	    }
	}
	if (info->biosBase &&
	    (info->biosBase < (memType)(-1 << info->biosSize)))
	    xf86ErrorF(", BIOS @ 0x%08lx/%d", info->biosBase, info->biosSize);
	xf86ErrorF("\n");
    }
}

/*
 * fixPciSizeInfo() -- fix pci size info by testing it destructively
 * (if not already done), fix pciVideoInfo and entry in the resource
 * list.
 */
/*
 * Note: once we have OS support to read the sizes GetBaseSize() will
 * have to be wrapped by the OS layer. fixPciSizeInfo() should also
 * be wrapped by the OS layer to do nothing if the size is always
 * returned correctly by GetBaseSize(). It should however set validate
 * in pciVideoRec if validation is required. ValidatePci() also needs
 * to be wrapped by the OS layer. This may do nothing if the OS has
 * already taken care of validation. fixPciResource() may be moved to
 * OS layer with minimal changes. Once the wrapping layer is in place
 * the common level and drivers should not reference these functions
 * directly but thru the OS layer.
 */

static void
fixPciSizeInfo(int entityIndex)
{
    pciVideoPtr pvp;
    pciConfigPtr pcp;
    resPtr pAcc;
    PCITAG tag;
    int j;

    if (! (pvp = xf86GetPciInfoForEntity(entityIndex))) return;
    pcp = pvp->thisCard;
    tag = pcp->tag;

    for (j = 0; j < 6; j++) {
	if (pvp->validSize & (2 << j))
	    continue;
	pAcc = Acc;
	if (!PCI_MAP_IS_IO((&pcp->pci_base0)[j])) {
	    while (pAcc) {
		if (((pAcc->res_type & (ResPhysMask | ResBlock))
		     == (ResMem | ResBlock))
		    && (pAcc->block_begin == B2M(tag, pvp->memBase[j]))
		    && (pAcc->block_end == B2M(tag, pvp->memBase[j]
		    + SIZE(pvp->size[j])))) break;
		pAcc = pAcc->next;
	    }
	} else {
	    while (pAcc) {
		if (((pAcc->res_type & (ResPhysMask | ResBlock)) ==
		     (ResIo | ResBlock))
		    && (pAcc->block_begin == B2I(tag, pvp->ioBase[j]))
		    && (pAcc->block_end == B2I(tag, pvp->ioBase[j]
		    + SIZE(pvp->size[j])))) break;
		pAcc = pAcc->next;
	    }
	}
	pvp->size[j] = pciGetBaseSize(pvp->thisCard, j, TRUE, &pvp->validSize);
	if (pAcc) {
	    pAcc->block_end = pvp->memBase[j] ?
		B2M(tag, pvp->memBase[j] + SIZE(pvp->size[j]))
		: B2I(tag ,pvp->ioBase[j] + SIZE(pvp->size[j]));
	    pAcc->res_type &= ~ResEstimated;
	    pAcc->res_type |= ResBios;
	}
    }
    if (!(pvp->validSize & (2 << 6))) {
	pAcc = Acc;
	while (pAcc) {
	    if (((pAcc->res_type & (ResPhysMask | ResBlock)) ==
		 (ResMem | ResBlock))
		&& (pAcc->block_begin == B2M(tag, pvp->biosBase))
		    && (pAcc->block_end == B2M(tag, pvp->biosBase
		    + SIZE(pvp->biosSize)))) break;
	    pAcc = pAcc->next;
	}
	pvp->biosSize = pciGetBaseSize(pvp->thisCard, 6, TRUE, &pvp->validSize);
	if (pAcc) {
	    pAcc->block_end = B2M(tag, pvp->biosBase+SIZE(pvp->biosSize));
	    pAcc->res_type &= ~ResEstimated;
	    pAcc->res_type |= ResBios;
	}
    }
}

/*
 * IO enable/disable related routines for PCI
 */
#define pArg ((pciArg*)arg)
#define SETBITS PCI_CMD_IO_ENABLE
static void
pciIoAccessEnable(void* arg)
{
#ifdef DEBUG
    ErrorF("pciIoAccessEnable: 0x%05lx\n", *(PCITAG *)arg);
#endif
    pArg->ctrl |= SETBITS | PCI_CMD_MASTER_ENABLE;
    pciWriteLong(pArg->tag, PCI_CMD_STAT_REG, pArg->ctrl);
}

static void
pciIoAccessDisable(void* arg)
{
#ifdef DEBUG
    ErrorF("pciIoAccessDisable: 0x%05lx\n", *(PCITAG *)arg);
#endif
    pArg->ctrl &= ~SETBITS;
    pciWriteLong(pArg->tag, PCI_CMD_STAT_REG, pArg->ctrl);
}

#undef SETBITS
#define SETBITS (PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE)
static void
pciIo_MemAccessEnable(void* arg)
{
#ifdef DEBUG
    ErrorF("pciIo_MemAccessEnable: 0x%05lx\n", *(PCITAG *)arg);
#endif
    pArg->ctrl |= SETBITS | PCI_CMD_MASTER_ENABLE;
    pciWriteLong(pArg->tag, PCI_CMD_STAT_REG, pArg->ctrl);
}

static void
pciIo_MemAccessDisable(void* arg)
{
#ifdef DEBUG
    ErrorF("pciIo_MemAccessDisable: 0x%05lx\n", *(PCITAG *)arg);
#endif
    pArg->ctrl &= ~SETBITS;
    pciWriteLong(pArg->tag, PCI_CMD_STAT_REG, pArg->ctrl);
}

#undef SETBITS
#define SETBITS (PCI_CMD_MEM_ENABLE)
static void
pciMemAccessEnable(void* arg)
{
#ifdef DEBUG
    ErrorF("pciMemAccessEnable: 0x%05lx\n", *(PCITAG *)arg);
#endif
    pArg->ctrl |= SETBITS | PCI_CMD_MASTER_ENABLE;
    pciWriteLong(pArg->tag, PCI_CMD_STAT_REG, pArg->ctrl);
}

static void
pciMemAccessDisable(void* arg)
{
#ifdef DEBUG
    ErrorF("pciMemAccessDisable: 0x%05lx\n", *(PCITAG *)arg);
#endif
    pArg->ctrl &= ~SETBITS;
    pciWriteLong(pArg->tag, PCI_CMD_STAT_REG, pArg->ctrl);
}
#undef SETBITS
#undef pArg


/* move to OS layer */
#define MASKBITS (PCI_PCI_BRIDGE_VGA_EN | PCI_PCI_BRIDGE_MASTER_ABORT_EN)
static void
pciBusAccessEnable(BusAccPtr ptr)
{
    PCITAG tag = ptr->busdep.pci.acc;
    CARD16 ctrl;

#ifdef DEBUG
    ErrorF("pciBusAccessEnable: bus=%d\n", ptr->busdep.pci.bus);
#endif
    ctrl = pciReadWord(tag, PCI_PCI_BRIDGE_CONTROL_REG);
    if ((ctrl & MASKBITS) != PCI_PCI_BRIDGE_VGA_EN) {
	ctrl = (ctrl | PCI_PCI_BRIDGE_VGA_EN) &
	    ~(PCI_PCI_BRIDGE_MASTER_ABORT_EN | PCI_PCI_BRIDGE_SECONDARY_RESET);
	pciWriteWord(tag, PCI_PCI_BRIDGE_CONTROL_REG, ctrl);
    }
}

/* move to OS layer */
static void
pciBusAccessDisable(BusAccPtr ptr)
{
    PCITAG tag = ptr->busdep.pci.acc;
    CARD16 ctrl;

#ifdef DEBUG
    ErrorF("pciBusAccessDisable: bus=%d\n", ptr->busdep.pci.bus);
#endif
    ctrl = pciReadWord(tag, PCI_PCI_BRIDGE_CONTROL_REG);
    if (ctrl & MASKBITS) {
	ctrl &= ~(MASKBITS | PCI_PCI_BRIDGE_SECONDARY_RESET);
	pciWriteWord(tag, PCI_PCI_BRIDGE_CONTROL_REG, ctrl);
    }
}
#undef MASKBITS

/* move to OS layer */
static void
pciDrvBusAccessEnable(BusAccPtr ptr)
{
    int bus = ptr->busdep.pci.bus;

#ifdef DEBUG
    ErrorF("pciDrvBusAccessEnable: bus=%d\n", bus);
#endif
    (*pciBusInfo[bus]->funcs->pciControlBridge)(bus,
						PCI_PCI_BRIDGE_VGA_EN,
						PCI_PCI_BRIDGE_VGA_EN);
}

/* move to OS layer */
static void
pciDrvBusAccessDisable(BusAccPtr ptr)
{
    int bus = ptr->busdep.pci.bus;

#ifdef DEBUG
    ErrorF("pciDrvBusAccessDisable: bus=%d\n", bus);
#endif
    (*pciBusInfo[bus]->funcs->pciControlBridge)(bus,
						PCI_PCI_BRIDGE_VGA_EN, 0);
}


static void
pciSetBusAccess(BusAccPtr ptr)
{
#ifdef DEBUG
    ErrorF("pciSetBusAccess: route VGA to bus %d\n", ptr->busdep.pci.bus);
#endif

    if (!ptr->primary && !ptr->current)
	return;

    if (ptr->current && ptr->current->disable_f)
	(*ptr->current->disable_f)(ptr->current);
    ptr->current = NULL;

    /* walk down */
    while (ptr->primary) {	/* No enable for root bus */
	if (ptr != ptr->primary->current) {
	    if (ptr->primary->current && ptr->primary->current->disable_f)
		(*ptr->primary->current->disable_f)(ptr->primary->current);
	    if (ptr->enable_f)
		(*ptr->enable_f)(ptr);
	    ptr->primary->current = ptr;
	}
	ptr = ptr->primary;
    }
}

/* move to OS layer */
static void
savePciState(PCITAG tag, pciSavePtr ptr)
{
    int i;

    ptr->command = pciReadLong(tag, PCI_CMD_STAT_REG);
    for (i=0; i < 6; i++)
	ptr->base[i] = pciReadLong(tag, PCI_MAP_REG_START + (i * 4));
    ptr->biosBase = pciReadLong(tag, PCI_MAP_ROM_REG);
}

/* move to OS layer */
static void
restorePciState(PCITAG tag, pciSavePtr ptr)
{
    int i;

    /* disable card before setting anything */
    pciSetBitsLong(tag, PCI_CMD_STAT_REG,
		   PCI_CMD_MEM_ENABLE | PCI_CMD_IO_ENABLE , 0);
    pciWriteLong(tag, PCI_MAP_ROM_REG, ptr->biosBase);
    for (i=0; i<6; i++)
	pciWriteLong(tag, PCI_MAP_REG_START + (i * 4), ptr->base[i]);
    pciWriteLong(tag, PCI_CMD_STAT_REG, ptr->command);
}

/* move to OS layer */
static void
savePciBusState(BusAccPtr ptr)
{
    PCITAG tag = ptr->busdep.pci.acc;

    ptr->busdep.pci.save.control =
	pciReadWord(tag, PCI_PCI_BRIDGE_CONTROL_REG) &
	~PCI_PCI_BRIDGE_SECONDARY_RESET;
    /* Allow master aborts to complete normally on non-root buses */
    if (ptr->busdep.pci.save.control & PCI_PCI_BRIDGE_MASTER_ABORT_EN)
	pciWriteWord(tag, PCI_PCI_BRIDGE_CONTROL_REG,
	    ptr->busdep.pci.save.control & ~PCI_PCI_BRIDGE_MASTER_ABORT_EN);
}

/* move to OS layer */
#define MASKBITS (PCI_PCI_BRIDGE_VGA_EN | PCI_PCI_BRIDGE_MASTER_ABORT_EN)
static void
restorePciBusState(BusAccPtr ptr)
{
    PCITAG tag = ptr->busdep.pci.acc;
    CARD16 ctrl;

    /* Only restore the bits we've changed (and don't cause resets) */
    ctrl = pciReadWord(tag, PCI_PCI_BRIDGE_CONTROL_REG);
    if ((ctrl ^ ptr->busdep.pci.save.control) & MASKBITS) {
	ctrl &= ~(MASKBITS | PCI_PCI_BRIDGE_SECONDARY_RESET);
	ctrl |= ptr->busdep.pci.save.control & MASKBITS;
	pciWriteWord(tag, PCI_PCI_BRIDGE_CONTROL_REG, ctrl);
    }
}
#undef MASKBITS

/* move to OS layer */
static void
savePciDrvBusState(BusAccPtr ptr)
{
    int bus = ptr->busdep.pci.bus;

    ptr->busdep.pci.save.control =
	(*pciBusInfo[bus]->funcs->pciControlBridge)(bus, 0, 0);
    /* Allow master aborts to complete normally on this bus */
    (*pciBusInfo[bus]->funcs->pciControlBridge)(bus,
						PCI_PCI_BRIDGE_MASTER_ABORT_EN,
						0);
}

/* move to OS layer */
static void
restorePciDrvBusState(BusAccPtr ptr)
{
    int bus = ptr->busdep.pci.bus;

    (*pciBusInfo[bus]->funcs->pciControlBridge)(bus, (CARD16)(-1),
						ptr->busdep.pci.save.control);
}


static void
disablePciBios(PCITAG tag)
{
    pciSetBitsLong(tag, PCI_MAP_ROM_REG, PCI_MAP_ROM_DECODE_ENABLE, 0);
}

/* ????? */
static void
correctPciSize(memType base, memType oldsize, memType newsize,
	       unsigned long type)
{
    pciConfigPtr pcrp, *pcrpp;
    pciVideoPtr pvp, *pvpp;
    CARD32 *basep;
    int i, numbars;
    int old_bits = 0, new_bits = 0;

    if (oldsize + 1) while (oldsize & 1) {
	old_bits ++;
	oldsize >>= 1;
    }
    if (newsize + 1) while (newsize & 1) {
	new_bits ++;
	newsize >>= 1;
    }

    for (pcrpp = xf86PciInfo, pcrp = *pcrpp; pcrp; pcrp = *++(pcrpp)) {

	/* Only process devices with type 0 headers */
	switch (pcrp->pci_header_type & 0x7f) {
	case 0:
	    numbars = 6;
	    break;

	case 1:
	    numbars = 2;
	    break;

	case 2:
	    numbars = 1;
	    break;

	default:
	    continue;
	}

	basep = &pcrp->pci_base0;
	for (i = 0; i < numbars; i++) {
	    if (basep[i] && (pcrp->basesize[i] == old_bits)) {
		if ((((type & ResPhysMask) == ResIo) &&
		     PCI_MAP_IS_IO(basep[i]) &&
		     B2I(pcrp->tag,PCIGETIO(basep[i]) == base)) ||
		    (((type & ResPhysMask) == ResMem) &&
		     PCI_MAP_IS_MEM(basep[i]) &&
		     (((!PCI_MAP_IS64BITMEM(basep[i]) || i == (numbars - 1)) &&
		       (B2M(pcrp->tag,PCIGETMEMORY(basep[i])) == base))
#if defined(LONG64) || defined(WORD64)
		      ||
		      (B2M(pcrp->tag,PCIGETMEMORY64(basep[i])) == base)
#else
		      ||
		      (!basep[i+1]
		       && (B2M(pcrp->tag,PCIGETMEMORY(basep[i])) == base))
#endif
		     ))) {
		    pcrp->basesize[i] = new_bits;
		    break;	/* to next device */
		}
	    }
	    if (PCI_MAP_IS64BITMEM(basep[i])) i++;
	}
    }

    if (xf86PciVideoInfo) {
	for (pvpp = xf86PciVideoInfo, pvp = *pvpp; pvp; pvp = *(++pvpp)) {

	    for (i = 0; i < 6; i++) {
		if (pvp->size[i] == old_bits) {
		    if ((((type & ResPhysMask) == ResIo)
			 && (B2I(TAG(pvp),pvp->ioBase[i]) == base)) ||
			(((type & ResPhysMask) == ResMem)
			  && (B2M(TAG(pvp),pvp->memBase[i]) == base))) {
			pvp->size[i] = new_bits;
			break;	/* to next device */
		    }
		}
	    }
	}
    }
}

/* ????? */
static void
removeOverlapsWithBridges(int busIndex, resPtr target)
{
    PciBusPtr pbp;
    resPtr tmp,bridgeRes = NULL;
    resRange range;

    if (!target)
	return;

    if (!ResCanOverlap(&target->val))
	return;

    range = target->val;

    for (pbp=xf86PciBus; pbp; pbp = pbp->next) {
	if (pbp->primary == busIndex) {
	    tmp = xf86DupResList(pbp->preferred_io);
	    bridgeRes = xf86JoinResLists(tmp,bridgeRes);
	    tmp = xf86DupResList(pbp->preferred_mem);
	    bridgeRes = xf86JoinResLists(tmp,bridgeRes);
	    tmp = xf86DupResList(pbp->preferred_pmem);
	    bridgeRes = xf86JoinResLists(tmp,bridgeRes);
	}
    }

    RemoveOverlaps(target, bridgeRes, TRUE, TRUE);
    if (range.rEnd > target->block_end) {
	correctPciSize(range.rBegin, range.rEnd - range.rBegin,
		       target->block_end - target->block_begin,
		       target->res_type);
	xf86MsgVerb(X_INFO, 3,
	    "PCI %s resource overlap reduced 0x%08lx from 0x%08lx to 0x%08lx\n",
	    ((target->res_type & ResPhysMask) == ResMem) ?  "Memory" : "I/O",
	    range.rBegin, range.rEnd, target->block_end);
    }
    xf86FreeResList(bridgeRes);
}

/* ????? */
static void
xf86GetPciRes(resPtr *activeRes, resPtr *inactiveRes)
{
    pciConfigPtr pcrp, *pcrpp;
    pciVideoPtr pvp, *pvpp;
    CARD32 *basep;
    int i;
    resPtr pRes, tmp;
    resRange range;
    long resMisc, resMisc2;

    if (activeRes)
	*activeRes = NULL;
    if (inactiveRes)
	*inactiveRes = NULL;

    if (!activeRes || !inactiveRes || !xf86PciInfo)
	return;

    if (xf86PciVideoInfo)
	for (pvpp = xf86PciVideoInfo, pvp = *pvpp; pvp; pvp = *(++pvpp)) {
	    resPtr *res;

	    if (PCINONSYSTEMCLASSES(pvp->class, pvp->subclass))
		resMisc = ResBios;
	    else
		resMisc = 0;

	    if (((pciConfigPtr)pvp->thisCard)->pci_command
		& (PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE))
		res = activeRes;
	    else
		res = inactiveRes;

	    for (i = 0; i < 6; i++) {
		if (pvp->size[i] == 0)
		    continue;

		if (pvp->validSize & (2 << i))
		    resMisc2 = 0;
		else
		    resMisc2 = ResEstimated;

		if (pvp->ioBase[i] &&
		    (pvp->ioBase[i] < (memType)(-1 << pvp->size[i]))) {
		    PV_I_RANGE(range, pvp, i,
			ResExcIoBlock | resMisc | resMisc2);
		    tmp = xf86AddResToList(NULL, &range, -1);
		    removeOverlapsWithBridges(pvp->bus, tmp);
		    *res = xf86JoinResLists(tmp, *res);
		} else if (pvp->memBase[i] &&
		    (pvp->memBase[i] < (memType)(-1 << pvp->size[i]))) {
		    PV_M_RANGE(range, pvp, i,
			ResExcMemBlock | resMisc | resMisc);
		    tmp = xf86AddResToList(NULL, &range, -1);
		    removeOverlapsWithBridges(pvp->bus, tmp);
		    *res = xf86JoinResLists(tmp, *res);
		}
	    }
	    /* FIXME!!!: Don't use BIOS resources for overlap
	     * checking but reserve them!
	     */
	    if (pvp->biosBase &&
		(pvp->biosBase < (memType)(-1 << pvp->biosSize))) {
		if (pvp->validSize & (2 << 6))
		    resMisc2 = 0;
		else
		    resMisc2 = ResEstimated;
		PV_B_RANGE(range, pvp, ResExcMemBlock | resMisc | resMisc2);
		tmp = xf86AddResToList(NULL, &range, -1);
		removeOverlapsWithBridges(pvp->bus, tmp);
		*res = xf86JoinResLists(tmp, *res);
	    }
	}

    for (pcrpp = xf86PciInfo, pcrp = *pcrpp; pcrp; pcrp = *++(pcrpp)) {
	resPtr *res;
	CARD8 baseclass, subclass, header_type, numbars;

	if (!(pcrp->pci_command & (PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE)))
	    continue;

	if (pcrp->listed_class & 0x0ffff) {
	    baseclass = pcrp->listed_class >> 8;
	    subclass = pcrp->listed_class;
	} else {
	    baseclass = pcrp->pci_base_class;
	    subclass = pcrp->pci_sub_class;
	}

	if (PCIINFOCLASSES(baseclass, subclass))
	    continue;

	header_type = pcrp->pci_header_type & 0x7f;
	switch (header_type) {
	case 0:
	    numbars = 6;
	    break;

	case 1:
	    numbars = 2;
	    break;

	case 2:
	    numbars = 1;
	    break;

	default:
	    continue;
	}

	/*
	 * Allow resources allocated to host bridges to overlap.  Perhaps, this
	 * needs to be specific to AGP-capable chipsets.  AGP "memory"
	 * sometimes gets allocated within the range routed to the AGP bus.
	 */
	if ((baseclass == PCI_CLASS_BRIDGE) &&
	    (subclass == PCI_SUBCLASS_BRIDGE_HOST))
	    resMisc = ResOverlap;
	else
	    resMisc = 0;

	basep = &pcrp->pci_base0;
	for (i = 0; i < numbars; i++) {
	    if (pcrp->basesize[i]) {
		if (pcrp->minBasesize & (2 << i))
		    resMisc2 = 0;
		else
		    resMisc2 = ResEstimated;
		if (PCI_MAP_IS_IO(basep[i])) {
		    if (pcrp->pci_command & PCI_CMD_IO_ENABLE)
			res = activeRes;
		    else
			res = inactiveRes;
		    P_I_RANGE(range, pcrp->tag, PCIGETIO(basep[i]),
			      pcrp->basesize[i],
			      ResExcIoBlock | resMisc | resMisc2)
		} else if (!PCI_MAP_IS64BITMEM(basep[i])) {
		    if (pcrp->pci_command & PCI_CMD_MEM_ENABLE)
			res = activeRes;
		    else
			res = inactiveRes;
		    P_M_RANGE(range, pcrp->tag, PCIGETMEMORY(basep[i]),
			      pcrp->basesize[i],
			      ResExcMemBlock | resMisc | resMisc2)
		} else {
		    i++;
		    if (i >= numbars)
			break;
#if defined(LONG64) || defined(WORD64)
		    P_M_RANGE(range,pcrp->tag,PCIGETMEMORY64(basep[i - 1]),
			      pcrp->basesize[i - 1],
			      ResExcMemBlock | resMisc | resMisc2)
#else
		    if (basep[i])
		      continue;
		    P_M_RANGE(range, pcrp->tag, PCIGETMEMORY(basep[i - 1]),
			      pcrp->basesize[i - 1],
			      ResExcMemBlock | resMisc | resMisc2)
#endif
		    if (pcrp->pci_command & PCI_CMD_MEM_ENABLE)
			res = activeRes;
		    else
			res = inactiveRes;
		}
		if (range.rBegin) { /* catch cases where PCI base is unset */
		    tmp = xf86AddResToList(NULL, &range, -1);
		    removeOverlapsWithBridges(pcrp->busnum, tmp);
		    *res = xf86JoinResLists(tmp, *res);
		}
	    }
	}

	/* Ignore disabled non-video ROMs */
	if ((pcrp->pci_command & PCI_CMD_MEM_ENABLE) &&
	    (pcrp->basesize[6] > 0)) {
	    switch (header_type) {
	    case 0:
		if (!(pcrp->pci_baserom & PCI_MAP_ROM_DECODE_ENABLE))
		    break;

		if (pcrp->minBasesize & (2 << 6))
		    resMisc2 = 0;
		else
		    resMisc2 = ResEstimated;
		P_M_RANGE(range, pcrp->tag, PCIGETROM(pcrp->pci_baserom),
			  pcrp->basesize[6],
			  ResExcMemBlock | resMisc | resMisc2);
		tmp = xf86AddResToList(NULL, &range, -1);
		removeOverlapsWithBridges(pcrp->busnum, tmp);
		*activeRes = xf86JoinResLists(tmp, *activeRes);
		break;

	    case 1:
		if (!(pcrp->pci_br_rom & PCI_MAP_ROM_DECODE_ENABLE))
		    break;

		if (pcrp->minBasesize & (2 << 6))
		    resMisc2 = 0;
		else
		    resMisc2 = ResEstimated;
		P_M_RANGE(range, pcrp->tag, PCIGETROM(pcrp->pci_br_rom),
			  pcrp->basesize[6],
			  ResExcMemBlock | resMisc | resMisc2);
		tmp = xf86AddResToList(NULL, &range, -1);
		removeOverlapsWithBridges(pcrp->busnum, tmp);
		*activeRes = xf86JoinResLists(tmp, *activeRes);
		break;

	    default:
		break;
	    }
	}
    }

    if (*activeRes) {
	xf86MsgVerb(X_INFO, 3, "Active PCI resource ranges:\n");
	xf86PrintResList(3, *activeRes);
    }
    if (*inactiveRes) {
	xf86MsgVerb(X_INFO, 3, "Inactive PCI resource ranges:\n");
	xf86PrintResList(3, *inactiveRes);
    }

    /*
     * Adjust ranges based on the assumption that there are no real
     * overlaps in the PCI base allocations.  This assumption should be
     * reasonable in most cases.  It may be possible to refine the
     * approximated PCI base sizes by considering bus mapping information
     * from PCI-PCI bridges.
     */

    if (*activeRes) {
	/* Check for overlaps */
	for (pRes = *activeRes; pRes; pRes = pRes->next) {
	    if (ResCanOverlap(&pRes->val)) {
		range = pRes->val;

		RemoveOverlaps(pRes, *activeRes, TRUE, TRUE);
		RemoveOverlaps(pRes, *inactiveRes, TRUE,
		    (xf86Info.estimateSizesAggressively > 0));

		if (range.rEnd > pRes->block_end) {
		    correctPciSize(range.rBegin, range.rEnd - range.rBegin,
				   pRes->block_end - pRes->block_begin,
				   pRes->res_type);
		    xf86MsgVerb(X_INFO, 3,
				"PCI %s resource overlap reduced 0x%08lx"
				" from 0x%08lx to 0x%08lx\n",
				((pRes->res_type & ResPhysMask) == ResMem) ?
				 "Memory" : "I/O",
				range.rBegin, range.rEnd, pRes->block_end);
		}
	    }
	}
	xf86MsgVerb(X_INFO, 3,
	    "Active PCI resource ranges after removing overlaps:\n");
	xf86PrintResList(3, *activeRes);
    }

    if (*inactiveRes) {
	/* Check for overlaps */
	for (pRes = *inactiveRes; pRes; pRes = pRes->next) {
	    if (ResCanOverlap(&pRes->val)) {
		range = pRes->val;

		RemoveOverlaps(pRes, *activeRes, TRUE,
		    (xf86Info.estimateSizesAggressively > 1));
		RemoveOverlaps(pRes, *inactiveRes, TRUE,
		    (xf86Info.estimateSizesAggressively > 1));

		if (range.rEnd > pRes->block_end) {
		    correctPciSize(range.rBegin, range.rEnd - range.rBegin,
				   pRes->block_end - pRes->block_begin,
				   pRes->res_type);
		    xf86MsgVerb(X_INFO, 3,
				"PCI %s resource overlap reduced 0x%08lx"
				" from 0x%08lx to 0x%08lx\n",
				((pRes->res_type & ResPhysMask) == ResMem) ?
				 "Memory" : "I/O",
				range.rBegin, range.rEnd, pRes->block_end);
		}

	    }
	}
	xf86MsgVerb(X_INFO, 3,
	    "Inactive PCI resource ranges after removing overlaps:\n");
	xf86PrintResList(3, *inactiveRes);
    }
}

resPtr
ResourceBrokerInitPci(resPtr *osRes)
{
    resPtr activeRes, inactiveRes;
    resPtr tmp;

    /* Get bus-specific system resources (PCI) */
    xf86GetPciRes(&activeRes, &inactiveRes);

    /*
     * Adjust OS-reported resource ranges based on the assumption that there
     * are no overlaps with the PCI base allocations.  This should be a good
     * assumption because writes to PCI address space won't be routed directly
     * to host memory.
     */

    for (tmp = *osRes; tmp; tmp = tmp->next)
	RemoveOverlaps(tmp, activeRes, FALSE, TRUE);

    xf86MsgVerb(X_INFO, 3, "OS-reported resource ranges after removing"
		" overlaps with PCI:\n");
    xf86PrintResList(3, *osRes);

    pciAvoidRes = xf86AddRangesToList(pciAvoidRes,PciAvoid,-1);
    for (tmp = pciAvoidRes; tmp; tmp = tmp->next)
	RemoveOverlaps(tmp, activeRes, FALSE, TRUE);
    tmp = xf86DupResList(*osRes);
    pciAvoidRes = xf86JoinResLists(pciAvoidRes,tmp);

    return (xf86JoinResLists(activeRes,inactiveRes));
}


/*
 * PCI Resource modification
 */
static Bool
fixPciResource(int prt, memType alignment, pciVideoPtr pvp, unsigned long type)
{
    int  res_n;
    memType *p_base;
    int *p_size;
    unsigned char p_type;
    resPtr AccTmp = NULL;
    resPtr orgAcc = NULL;
    resPtr *pAcc = &AccTmp;
    resPtr avoid = NULL;
    resRange range;
    resPtr resSize = NULL;
    resPtr w_tmp, w = NULL, w_2nd = NULL;
    PCITAG tag;
    PciBusPtr pbp = xf86PciBus;
    pciConfigPtr pcp;
    resPtr tmp;

    if (!pvp || (pvp->size[prt] <= 0)) return FALSE;
    pcp = pvp->thisCard;
    tag = pcp->tag;

    type &= ResAccMask;
    if (!type) type = ResShared;
    if (prt < 6) {
	if (!PCI_MAP_IS_IO((&pcp->pci_base0)[prt])) {
	    type |= ResMem;
	    res_n = prt;
	    p_base = &(pvp->memBase[res_n]);
	    p_size = &(pvp->size[res_n]);
	    p_type = pvp->type[res_n];
	    if (!PCI_MAP_IS64BITMEM(pvp->type[res_n])) {
	      PCI_M_RANGE(range,tag,0,0xffffffff,ResExcMemBlock);
	      resSize = xf86AddResToList(resSize,&range,-1);
	    }
	} else {
	    type |= ResIo;
	    res_n = prt;
	    p_base = &(pvp->ioBase[res_n]);
	    p_size = &(pvp->size[res_n]);
	    p_type = pvp->type[res_n];
	    PCI_I_RANGE(range, tag, 0, 0xffffffff, ResExcIoBlock);
	    resSize = xf86AddResToList(resSize, &range, -1);
	}
    } else if (prt == 6) {
	type |= ResMem;
	res_n = 0xff;	/* special flag for bios rom */
	p_base = &(pvp->biosBase);
	p_size = &(pvp->biosSize);
	/* XXX This should also include the PCI_MAP_MEMORY_TYPE_MASK part */
	p_type = 0;
	PCI_M_RANGE(range,tag,0,0xffffffff,ResExcMemBlock);
	resSize = xf86AddResToList(resSize,&range,-1);
    } else return FALSE;

    type |= (range.type & ResDomain) | ResBlock;

    /* setup avoid: PciAvoid is bus range: convert later */
    avoid = xf86DupResList(pciAvoidRes);

    while (pbp) {
	if (pbp->secondary == pvp->bus) {
	    if ((type & ResPhysMask) == ResMem) {
		if (((p_type & PCI_MAP_MEMORY_CACHABLE)
#if 0 /*EE*/
		     || (res_n == 0xff)/* bios should also be prefetchable */
#endif
		     )) {
		    if (pbp->preferred_pmem)
			w = xf86FindIntersectOfLists(pbp->preferred_pmem,
						     ResRange);
		    else if (pbp->pmem)
			w = xf86FindIntersectOfLists(pbp->pmem,ResRange);

		    if (pbp->preferred_mem)
			w_2nd = xf86FindIntersectOfLists(pbp->preferred_mem,
							 ResRange);
		    else if (pbp->mem)
			w_2nd = xf86FindIntersectOfLists(pbp->mem,
							 ResRange);
		} else {
		    if (pbp->preferred_mem)
			w = xf86FindIntersectOfLists(pbp->preferred_mem,
						     ResRange);
		    else if (pbp->mem)
			w = xf86FindIntersectOfLists(pbp->mem,ResRange);
		}
	    } else {
		if (pbp->preferred_io)
		    w = xf86FindIntersectOfLists(pbp->preferred_io,ResRange);
		if (pbp->io)
		    w = xf86FindIntersectOfLists(pbp->io,ResRange);
	    }
	} else if (pbp->primary == pvp->bus) {
	    if ((type & ResPhysMask) == ResMem) {
		tmp = xf86DupResList(pbp->preferred_pmem);
		avoid = xf86JoinResLists(avoid, tmp);
		tmp = xf86DupResList(pbp->preferred_mem);
		avoid = xf86JoinResLists(avoid, tmp);
	    } else {
		tmp = xf86DupResList(pbp->preferred_io);
		avoid = xf86JoinResLists(avoid, tmp);
	    }
	}
	pbp = pbp->next;
    }

    /* convert bus based entries in avoid list to host base */
    pciConvertListToHost(pvp->bus,pvp->device,pvp->func, avoid);

    if (!w)
	w = xf86DupResList(ResRange);
    xf86MsgVerb(X_INFO, 3, "window:\n");
    xf86PrintResList(3, w);
    xf86MsgVerb(X_INFO, 3, "resSize:\n");
    xf86PrintResList(3, resSize);

    if (resSize) {
	w_tmp = w;
	w = xf86FindIntersectOfLists(w,resSize);
	xf86FreeResList(w_tmp);
	if (w_2nd) {
	    w_tmp = w_2nd;
	    w_2nd = xf86FindIntersectOfLists(w_2nd,resSize);
	    xf86FreeResList(w_tmp);
	}
	xf86FreeResList(resSize);
    }
    xf86MsgVerb(X_INFO, 3, "window fixed:\n");
    xf86PrintResList(3, w);

    if (!alignment)
	alignment = (1 << (*p_size)) - 1;

    /* Access list holds bios resources -- remove this one */
#ifdef NOTYET
    AccTmp = xf86DupResList(Acc);
    while ((*pAcc)) {
	if ((((*pAcc)->res_type & (type & ~ResAccMask))
	     == (type & ~ResAccMask))
	    && ((*pAcc)->block_begin == (B2H(tag,(*p_base),type)))
	    && ((*pAcc)->block_end == (B2H(tag,
					   (*p_base)+SIZE(*p_size),type)))) {
	    resPtr acc_tmp = (*pAcc)->next;
	    xfree((*pAcc));
	    (*pAcc) = acc_tmp;
	    break;
	} else
	    pAcc = &((*pAcc)->next);
    }
    /* check if we really need to fix anything */
    P_X_RANGE(range,tag,(*p_base),(*p_base) + SIZE((*p_size)),type);
    if (!ChkConflict(&range,avoid,SETUP)
	&& !ChkConflict(&range,AccTmp,SETUP)
	&& ((B2H(tag,(*p_base),type) & PCI_SIZE(type,tag,alignment)
	     == range->block_begin)
	&& ((xf86IsSubsetOf(range,w)
	    || (w_2nd && xf86IsSubsetOf(range,w_2n))))) {
#ifdef DEBUG
	    ErrorF("nothing to fix\n");
#endif
	xf86FreeResList(AccTmp);
	xf86FreeResList(w);
	xf86FreeResList(w_2nd);
	xf86FreeResList(avoid);
	return TRUE;
    }
#ifdef DEBUG
	ErrorF("removing old resource\n");
#endif
    orgAcc = Acc;
    Acc = AccTmp;
#else
    orgAcc = xf86DupResList(Acc);
    pAcc = &Acc;
    while (*pAcc) {
	if ((((*pAcc)->res_type & (ResTypeMask|ResExtMask)) ==
	     (type & ~ResAccMask))
	    && ((*pAcc)->block_begin == B2H(tag,(*p_base),type))
	    && ((*pAcc)->block_end == B2H(tag,(*p_base) + SIZE(*p_size),
					  type))) {
#ifdef DEBUG
	    ErrorF("removing old resource\n");
#endif
	    tmp = *pAcc;
	    *pAcc = (*pAcc)->next;
	    tmp->next = NULL;
	    xf86FreeResList(tmp);
	    break;
	} else
	    pAcc = &((*pAcc)->next);
    }
#endif

#ifdef DEBUG
    ErrorF("base: 0x%lx alignment: 0x%lx host alignment: 0x%lx size[bit]: 0x%x\n",
	   (*p_base),alignment,PCI_SIZE(type,tag,alignment),(*p_size));
    xf86MsgVerb(X_INFO, 3, "window:\n");
    xf86PrintResList(3, w);
    if (w_2nd)
	xf86MsgVerb(X_INFO, 3, "2nd window:\n");
    xf86PrintResList(3, w_2nd);
    xf86ErrorFVerb(3,"avoid:\n");
    xf86PrintResList(3,avoid);
#endif
    w_tmp = w;
    while (w) {
	if ((type & ResTypeMask) == (w->res_type & ResTypeMask)) {
#ifdef DEBUG
	    ErrorF("block_begin: 0x%lx block_end: 0x%lx\n",w->block_begin,
		   w->block_end);
#endif
	    range = xf86GetBlock(type,PCI_SIZE(type,tag,alignment + 1),
				 w->block_begin, w->block_end,
				 PCI_SIZE(type,tag,alignment),avoid);
	    if (range.type != ResEnd)
		break;
	}
	w = w->next;
    }
    xf86FreeResList(w_tmp);
    /* if unsuccessful and memory prefetchable try non-prefetchable */
    if (range.type == ResEnd && w_2nd) {
	w_tmp = w_2nd;
	while (w_2nd) {
	    if ((type & ResTypeMask) == (w_2nd->res_type & ResTypeMask)) {
#ifdef DEBUG
	    ErrorF("block_begin: 0x%lx block_end: 0x%lx\n",w_2nd->block_begin,
		   w_2nd->block_end);
#endif
	    range = xf86GetBlock(type,PCI_SIZE(type,tag,alignment + 1),
				 w_2nd->block_begin, w_2nd->block_end,
				 PCI_SIZE(type,tag,alignment),avoid);
		if (range.type != ResEnd)
		    break;
	    }
	w_2nd = w_2nd->next;
	}
	xf86FreeResList(w_tmp);
    }
    xf86FreeResList(avoid);

    if (range.type == ResEnd) {
	xf86MsgVerb(X_ERROR,3,"Cannot find a replacement memory range\n");
	xf86FreeResList(Acc);
	Acc = orgAcc;
	return FALSE;
    }
    xf86FreeResList(orgAcc);
#ifdef DEBUG
    ErrorF("begin: 0x%lx, end: 0x%lx\n",range.a,range.b);
#endif

    (*p_size) = 0;
    while (alignment >> (*p_size))
	(*p_size)++;
    (*p_base) = H2B(tag,range.rBegin,type);
#ifdef DEBUG
    ErrorF("New PCI res %i base: 0x%lx, size: 0x%lx, type %s\n",
	   res_n, *p_base, (1UL << *p_size),
	   ((type & ResPhysMask) == ResMem) ? "Mem" : "Io");
#endif
    if (res_n != 0xff) {
	if ((type & ResPhysMask) == ResMem)
	    pvp->memBase[prt] = range.rBegin;
	else
	    pvp->ioBase[prt] = range.rBegin;
	((CARD32 *)(&(pcp->pci_base0)))[res_n] =
	    (CARD32)(*p_base) | (CARD32)(p_type);
	pciWriteLong(tag, PCI_MAP_REG_START + (res_n * sizeof(CARD32)),
		     ((CARD32 *)(&(pcp->pci_base0)))[res_n]);
	if (PCI_MAP_IS64BITMEM(p_type) && (res_n < 5)) {
#if defined(LONG64) || defined(WORD64)
	    ((CARD32 *)(&(pcp->pci_base0)))[res_n + 1] =
		(CARD32)(*p_base >> 32);
	    pciWriteLong(tag, PCI_MAP_REG_START + ((res_n + 1) * sizeof(CARD32)),
			 ((CARD32 *)(&(pcp->pci_base0)))[res_n + 1]);
#else
	    ((CARD32 *)(&(pcp->pci_base0)))[res_n + 1] = 0;
	    pciWriteLong(tag, PCI_MAP_REG_START + ((res_n + 1) * sizeof(CARD32)),
			 0);
#endif
	}
    } else {
	pvp->biosBase = range.rBegin;
	pcp->pci_baserom =
	    (pciReadLong(tag, PCI_MAP_ROM_REG) & PCI_MAP_ROM_DECODE_ENABLE) |
	    (CARD32)(*p_base);
	pciWriteLong(tag, PCI_MAP_ROM_REG, pcp->pci_baserom);
    }
    /* @@@ fake BIOS allocated resource */
    range.type |= ResBios;
    w = xf86AddResToList(NULL, &range, -1);
    xf86MsgVerb(X_INFO, 3, "Resource relocated to:\n");
    xf86PrintResList(3, w);
    Acc = xf86JoinResLists(w, Acc);

    return TRUE;

}

Bool
xf86FixPciResource(int entityIndex, int prt, memType alignment,
		   unsigned long type)
{
    pciVideoPtr pvp = xf86GetPciInfoForEntity(entityIndex);
    return fixPciResource(prt, alignment, pvp, type);
}

resPtr
xf86ReallocatePciResources(int entityIndex, resPtr pRes)
{
    pciVideoPtr pvp = xf86GetPciInfoForEntity(entityIndex);
    resPtr pBad = NULL,pResTmp;
    unsigned int prt = 0;
    int i;

    if (!pvp) return pRes;

    while (pRes) {
	switch (pRes->res_type & ResPhysMask) {
	case ResMem:
	    if (pRes->block_begin == B2M(TAG(pvp),pvp->biosBase) &&
		pRes->block_end == B2M(TAG(pvp),pvp->biosBase
				       + SIZE(pvp->biosSize)))
		prt = 6;
	    else for (i = 0 ; i < 6; i++)
		if ((pRes->block_begin == B2M(TAG(pvp),pvp->memBase[i]))
		    && (pRes->block_end == B2M(TAG(pvp),pvp->memBase[i]
					      + SIZE(pvp->size[i])))) {
		    prt = i;
		    break;
		}
	    break;
	case ResIo:
	    for (i = 0 ; i < 6; i++)
		if (pRes->block_begin == B2I(TAG(pvp),pvp->ioBase[i])
		    && pRes->block_end == B2I(TAG(pvp),pvp->ioBase[i]
		    + SIZE(pvp->size[i]))) {
		    prt = i;
		    break;
		}
	    break;
	}

	if (!prt) return pRes;

	pResTmp = pRes->next;
	if (! fixPciResource(prt, 0, pvp, pRes->res_type)) {
	    pRes->next = pBad;
	    pBad = pRes;
	} else
	    xfree(pRes);

	pRes = pResTmp;
    }
    return pBad;
}

/*
 * BIOS releated
 */
memType
getValidBIOSBase(PCITAG tag, int num)
{
    pciVideoPtr pvp = NULL;
    PciBusPtr pbp;
    resPtr m = NULL;
    resPtr tmp, avoid, mem = NULL;
    resRange range;
    memType ret;
    int n = 0;
    int i;
    CARD32 biosSize, alignment;

    if (!xf86PciVideoInfo) return 0;

    while ((pvp = xf86PciVideoInfo[n++])) {
	if (pciTag(pvp->bus,pvp->device,pvp->func) == tag)
	    break;
    }
    if (!pvp) return 0;

    biosSize = pvp->biosSize;
    alignment = (1 << biosSize) - 1;
    if (biosSize > 24)
	biosSize = 24;

    switch ((romBaseSource)num) {
    case ROM_BASE_PRESET:
	return 0; /* This should not happen */
    case ROM_BASE_BIOS:
	/* In some cases the BIOS base register contains the size mask */
	if ((memType)(-1 << biosSize) == PCIGETROM(pvp->biosBase))
	    return 0;
	/* Make sure we don't conflict with our own mem resources */
	for (i = 0; i < 6; i++) {
	    if (!pvp->memBase[i])
		continue;
	    P_M_RANGE(range,TAG(pvp),pvp->memBase[i],pvp->size[i],
		      ResExcMemBlock);
	    mem = xf86AddResToList(mem,&range,-1);
	}
	P_M_RANGE(range, TAG(pvp),pvp->biosBase,biosSize,ResExcMemBlock);
	ret = pvp->biosBase;
	break;
    case ROM_BASE_MEM0:
    case ROM_BASE_MEM1:
    case ROM_BASE_MEM2:
    case ROM_BASE_MEM3:
    case ROM_BASE_MEM4:
    case ROM_BASE_MEM5:
	if (!pvp->memBase[num] || (pvp->size[num] < biosSize))
	    return 0;
	P_M_RANGE(range, TAG(pvp),pvp->memBase[num],biosSize,
		  ResExcMemBlock);
	ret = pvp->memBase[num];
	break;
    case ROM_BASE_FIND:
	ret = 0;
	break;
    default:
	return 0; /* This should not happen */
    }

    /* Now find the ranges for validation */
    avoid = xf86DupResList(pciAvoidRes);
    pbp = xf86PciBus;
    while (pbp) {
	if (pbp->secondary == pvp->bus) {
	    if (pbp->preferred_pmem)
		tmp = xf86DupResList(pbp->preferred_pmem);
	    else
		tmp = xf86DupResList(pbp->pmem);
	    m = xf86JoinResLists(m,tmp);
	    if (pbp->preferred_mem)
		tmp = xf86DupResList(pbp->preferred_mem);
	    else
		tmp = xf86DupResList(pbp->mem);
	    m = xf86JoinResLists(m,tmp);
	    tmp = m;
	    while (tmp) {
		tmp->block_end = MIN(tmp->block_end,PCI_MEM32_LENGTH_MAX);
		tmp = tmp->next;
	    }
	} else if ((pbp->primary == pvp->bus) &&
		   (pbp->secondary >= 0) &&
		   (pbp->primary != pbp->secondary)) {
	    tmp = xf86DupResList(pbp->preferred_pmem);
	    avoid = xf86JoinResLists(avoid, tmp);
	    tmp = xf86DupResList(pbp->pmem);
	    avoid = xf86JoinResLists(avoid, tmp);
	    tmp = xf86DupResList(pbp->preferred_mem);
	    avoid = xf86JoinResLists(avoid, tmp);
	    tmp = xf86DupResList(pbp->mem);
	    avoid = xf86JoinResLists(avoid, tmp);
	}
	pbp = pbp->next;
    }
    pciConvertListToHost(pvp->bus,pvp->device,pvp->func, avoid);
    if (mem)
	pciConvertListToHost(pvp->bus,pvp->device,pvp->func, mem);

    if (!ret) {
	/* Return a possible window */
	while (m) {
	    range = xf86GetBlock(RANGE_TYPE(ResExcMemBlock, xf86GetPciDomain(tag)),
				 PCI_SIZE(ResMem, TAG(pvp), 1 << biosSize),
				 m->block_begin, m->block_end,
				 PCI_SIZE(ResMem, TAG(pvp), alignment),
				 avoid);
	    if (range.type != ResEnd) {
		ret =  M2B(TAG(pvp), range.rBase);
		break;
	    }
	    m = m->next;
	}
    } else {
	if (!xf86IsSubsetOf(range, m) ||
	    ChkConflict(&range, avoid, SETUP)
	    || (mem && ChkConflict(&range, mem, SETUP)))
	    ret = 0;
    }

    xf86FreeResList(avoid);
    xf86FreeResList(m);
    return ret;
}

/*
 * xf86Bus.c interface
 */

void
xf86PciProbe(void)
{
    /*
     * Initialise the pcidata entry points.
     */
#ifdef XFree86LOADER
    xf86SetupPciIds = (ScanPciSetupProcPtr)LoaderSymbol("ScanPciSetupPciIds");
    xf86ClosePciIds = (ScanPciCloseProcPtr)LoaderSymbol("ScanPciClosePciIds");
    xf86FindPciNamesByDevice =
	(ScanPciFindByDeviceProcPtr)LoaderSymbol("ScanPciFindPciNamesByDevice");
    xf86FindPciNamesBySubsys =
	(ScanPciFindBySubsysProcPtr)LoaderSymbol("ScanPciFindPciNamesBySubsys");
    xf86FindPciClassBySubsys =
	(ScanPciFindClassBySubsysProcPtr)LoaderSymbol("ScanPciFindPciClassBySubsys");
    xf86FindPciClassByDevice =
	(ScanPciFindClassByDeviceProcPtr)LoaderSymbol("ScanPciFindPciClassByDevice");
#else
    xf86SetupPciIds = ScanPciSetupPciIds;
    xf86ClosePciIds = ScanPciClosePciIds;
    xf86FindPciNamesByDevice = ScanPciFindPciNamesByDevice;
    xf86FindPciNamesBySubsys = ScanPciFindPciNamesBySubsys;
    xf86FindPciClassBySubsys = ScanPciFindPciClassBySubsys;
    xf86FindPciClassByDevice = ScanPciFindPciClassByDevice;
#endif

    if (!xf86SetupPciIds())
	FatalError("xf86SetupPciIds() failed\n");

    FindPCIVideoInfo();
}

static void alignBridgeRanges(PciBusPtr PciBusBase, PciBusPtr primary);

static void
printBridgeInfo(PciBusPtr PciBus)
{
    char primary[8], secondary[8], subordinate[8], brbus[8];

    xf86FormatPciBusNumber(PciBus->primary, primary);
    xf86FormatPciBusNumber(PciBus->secondary, secondary);
    xf86FormatPciBusNumber(PciBus->subordinate, subordinate);
    xf86FormatPciBusNumber(PciBus->brbus, brbus);

    xf86MsgVerb(X_INFO, 3, "Bus %s: bridge is at (%s:%d:%d), (%s,%s,%s),"
		" BCTRL: 0x%04x (VGA_EN is %s)\n",
		secondary, brbus, PciBus->brdev, PciBus->brfunc,
		primary, secondary, subordinate, PciBus->brcontrol,
		(PciBus->brcontrol & PCI_PCI_BRIDGE_VGA_EN) ?
		 "set" : "cleared");
    if (PciBus->preferred_io) {
	xf86MsgVerb(X_INFO, 3,
		    "Bus %s I/O range:\n", secondary);
	xf86PrintResList(3, PciBus->preferred_io);
    }
    if (PciBus->preferred_mem) {
	xf86MsgVerb(X_INFO, 3,
		    "Bus %s non-prefetchable memory range:\n", secondary);
	xf86PrintResList(3, PciBus->preferred_mem);
    }
    if (PciBus->preferred_pmem) {
	xf86MsgVerb(X_INFO, 3,
		    "Bus %s prefetchable memory range:\n", secondary);
	xf86PrintResList(3, PciBus->preferred_pmem);
    }
}

static PciBusPtr
xf86GetPciBridgeInfo(void)
{
    const pciConfigPtr *pcrpp;
    pciConfigPtr pcrp;
    pciBusInfo_t *pBusInfo;
    resRange range;
    PciBusPtr PciBus, PciBusBase = NULL;
    PciBusPtr *pnPciBus = &PciBusBase;
    int MaxBus = 0;
    int i, domain;
    int primary, secondary, subordinate;
    memType base, limit;

    resPtr pciBusAccWindows = xf86PciBusAccWindowsFromOS();

    if (xf86PciInfo == NULL)
	return NULL;

    /* Add each bridge */
    for (pcrpp = xf86PciInfo, pcrp = *pcrpp; pcrp; pcrp = *(++pcrpp)) {
	if (pcrp->busnum > MaxBus)
	    MaxBus = pcrp->busnum;
	if ((pcrp->pci_base_class == PCI_CLASS_BRIDGE) ||
	    (((pcrp->listed_class >> 8) & 0xff) == PCI_CLASS_BRIDGE)) {
	    int sub_class;
	    sub_class = (pcrp->listed_class & 0xffff) ?
		(pcrp->listed_class & 0xff) : pcrp->pci_sub_class;
	    domain = xf86GetPciDomain(pcrp->tag);

	    switch (sub_class) {
	    case PCI_SUBCLASS_BRIDGE_PCI:
		/* something fishy about the header? If so: just ignore! */
		if ((pcrp->pci_header_type & 0x7f) != 0x01) {
		    xf86MsgVerb(X_WARNING, 3, "PCI-PCI bridge at %x:%x:%x has"
				" unexpected header:  0x%x",
				pcrp->busnum, pcrp->devnum,
				pcrp->funcnum, pcrp->pci_header_type);
		    break;
		}

		i = pcrp->busnum & (PCI_DOM_MASK << 8);
		primary = pcrp->busnum;
		secondary = i | pcrp->pci_secondary_bus_number;
		subordinate = i | pcrp->pci_subordinate_bus_number;

		/* Is this the correct bridge? If not, ignore it */
		pBusInfo = pcrp->businfo;
		if (pBusInfo && (pcrp != pBusInfo->bridge)) {
		    xf86MsgVerb(X_WARNING, 3, "PCI bridge mismatch for bus %x:"
				" %x:%x:%x and %x:%x:%x\n", secondary,
				pcrp->busnum, pcrp->devnum, pcrp->funcnum,
				pBusInfo->bridge->busnum,
				pBusInfo->bridge->devnum,
				pBusInfo->bridge->funcnum);
		    break;
		}

		if (pBusInfo && pBusInfo->funcs->pciGetBridgeBuses)
		    (*pBusInfo->funcs->pciGetBridgeBuses)(secondary,
							   &primary,
							   &secondary,
							   &subordinate);

		if (!pcrp->fakeDevice && (primary >= secondary)) {
		    xf86MsgVerb(X_WARNING, 3, "Misconfigured PCI bridge"
				" %x:%x:%x (%x,%x)\n",
				pcrp->busnum, pcrp->devnum, pcrp->funcnum,
				primary, secondary);
		    break;
		}

		*pnPciBus = PciBus = xnfcalloc(1, sizeof(PciBusRec));
		pnPciBus = &PciBus->next;

		PciBus->domain = domain;
		PciBus->primary = primary;
		PciBus->secondary = secondary;
		PciBus->subordinate = subordinate;

		PciBus->brbus = pcrp->busnum;
		PciBus->brdev = pcrp->devnum;
		PciBus->brfunc = pcrp->funcnum;

		PciBus->subclass = sub_class;
		PciBus->interface = pcrp->pci_prog_if;

		if (pBusInfo && pBusInfo->funcs->pciControlBridge)
		    PciBus->brcontrol =
			(*pBusInfo->funcs->pciControlBridge)(secondary, 0, 0);
		else
		    PciBus->brcontrol = pcrp->pci_bridge_control;

		if (pBusInfo && pBusInfo->funcs->pciGetBridgeResources) {
		    (*pBusInfo->funcs->pciGetBridgeResources)(secondary,
			(pointer *)&PciBus->preferred_io,
			(pointer *)&PciBus->preferred_mem,
			(pointer *)&PciBus->preferred_pmem);
		    break;
		}

		if ((pcrp->pci_command & PCI_CMD_IO_ENABLE) &&
		    (pcrp->pci_upper_io_base || pcrp->pci_io_base ||
		     pcrp->pci_upper_io_limit || pcrp->pci_io_limit)) {
		    base = (pcrp->pci_upper_io_base << 16) |
			((pcrp->pci_io_base & 0xf0u) << 8);
		    limit = (pcrp->pci_upper_io_limit << 16) |
			((pcrp->pci_io_limit & 0xf0u) << 8) | 0x0fff;
		    /*
		     * Deal with bridge ISA mode (256 wide ranges spaced 1K
		     * apart, but only in the first 64K).
		     */
		    if (pcrp->pci_bridge_control & PCI_PCI_BRIDGE_ISA_EN) {
			while ((base <= (CARD16)(-1)) && (base <= limit)) {
			    PCI_I_RANGE(range, pcrp->tag,
				base, base + (CARD8)(-1),
				ResIo | ResBlock | ResExclusive);
			    PciBus->preferred_io =
				xf86AddResToList(PciBus->preferred_io,
						 &range, -1);
			    base += 0x0400;
			}
		    }
		    if (base <= limit) {
			PCI_I_RANGE(range, pcrp->tag, base, limit,
			    ResIo | ResBlock | ResExclusive);
			PciBus->preferred_io =
			    xf86AddResToList(PciBus->preferred_io, &range, -1);
		    }
		}
		if (pcrp->pci_command & PCI_CMD_MEM_ENABLE) {
		  /*
		   * The P2P spec requires these next two, but some bridges
		   * don't comply.  Err on the side of caution, making the not
		   * so bold assumption that no bridge would ever re-route the
		   * bottom megabyte.
		   */
		  if (pcrp->pci_mem_base || pcrp->pci_mem_limit) {
		    base = pcrp->pci_mem_base & 0xfff0u;
		    limit = pcrp->pci_mem_limit & 0xfff0u;
		    if (base <= limit) {
			PCI_M_RANGE(range, pcrp->tag,
				    base << 16, (limit << 16) | 0x0fffff,
				    ResMem | ResBlock | ResExclusive);
			PciBus->preferred_mem =
			    xf86AddResToList(PciBus->preferred_mem, &range, -1);
		    }
		  }

		  if (pcrp->pci_prefetch_mem_base ||
		      pcrp->pci_prefetch_mem_limit ||
		      pcrp->pci_prefetch_upper_mem_base ||
		      pcrp->pci_prefetch_upper_mem_limit) {
		    base = pcrp->pci_prefetch_mem_base & 0xfff0u;
		    limit = pcrp->pci_prefetch_mem_limit & 0xfff0u;
#if defined(LONG64) || defined(WORD64)
		    base |= (memType)pcrp->pci_prefetch_upper_mem_base << 16;
		    limit |= (memType)pcrp->pci_prefetch_upper_mem_limit << 16;
#else
		    if (pcrp->pci_prefetch_upper_mem_base)
			break;

		    if (pcrp->pci_prefetch_upper_mem_limit)
			limit = 0xfff0u;
#endif
		    if (base <= limit) {
			PCI_M_RANGE(range, pcrp->tag,
				    base << 16, (limit << 16) | 0xfffff,
				    ResMem | ResBlock | ResExclusive);
			PciBus->preferred_pmem =
			    xf86AddResToList(PciBus->preferred_pmem,
					     &range, -1);
		    }
		  }
		}
		break;

	    case PCI_SUBCLASS_BRIDGE_CARDBUS:
		/* something fishy about the header? If so: just ignore! */
		if ((pcrp->pci_header_type & 0x7f) != 0x02) {
		    xf86MsgVerb(X_WARNING, 3, "PCI-CardBus bridge at %x:%x:%x"
				" has unexpected header:  0x%x",
				pcrp->busnum, pcrp->devnum,
				pcrp->funcnum, pcrp->pci_header_type);
		    break;
		}

		i = pcrp->busnum & (PCI_DOM_MASK << 8);
		primary = pcrp->busnum;
		secondary = i | pcrp->pci_cb_cardbus_bus_number;
		subordinate = i | pcrp->pci_subordinate_bus_number;

		/* Is this the correct bridge?  If not, ignore it */
		pBusInfo = pcrp->businfo;
		if (pBusInfo && (pcrp != pBusInfo->bridge)) {
		    xf86MsgVerb(X_WARNING, 3, "CardBus bridge mismatch for bus"
				" %x: %x:%x:%x and %x:%x:%x\n", secondary,
				pcrp->busnum, pcrp->devnum, pcrp->funcnum,
				pBusInfo->bridge->busnum,
				pBusInfo->bridge->devnum,
				pBusInfo->bridge->funcnum);
		    break;
		}

		if (pBusInfo && pBusInfo->funcs->pciGetBridgeBuses)
		    (*pBusInfo->funcs->pciGetBridgeBuses)(secondary,
							   &primary,
							   &secondary,
							   &subordinate);

		if (primary >= secondary) {
		    if (pcrp->pci_cb_cardbus_bus_number != 0)
			xf86MsgVerb(X_WARNING, 3, "Misconfigured CardBus"
				    " bridge %x:%x:%x (%x,%x)\n",
				    pcrp->busnum, pcrp->devnum, pcrp->funcnum,
				    primary, secondary);
		    break;
		}

		*pnPciBus = PciBus = xnfcalloc(1, sizeof(PciBusRec));
		pnPciBus = &PciBus->next;

		PciBus->domain = domain;
		PciBus->primary = primary;
		PciBus->secondary = secondary;
		PciBus->subordinate = subordinate;

		PciBus->brbus = pcrp->busnum;
		PciBus->brdev = pcrp->devnum;
		PciBus->brfunc = pcrp->funcnum;

		PciBus->subclass = sub_class;
		PciBus->interface = pcrp->pci_prog_if;

		if (pBusInfo && pBusInfo->funcs->pciControlBridge)
		    PciBus->brcontrol =
			(*pBusInfo->funcs->pciControlBridge)(secondary, 0, 0);
		else
		    PciBus->brcontrol = pcrp->pci_bridge_control;

		if (pBusInfo && pBusInfo->funcs->pciGetBridgeResources) {
		    (*pBusInfo->funcs->pciGetBridgeResources)(secondary,
			(pointer *)&PciBus->preferred_io,
			(pointer *)&PciBus->preferred_mem,
			(pointer *)&PciBus->preferred_pmem);
		    break;
		}

		if (pcrp->pci_command & PCI_CMD_IO_ENABLE) {
		    if (pcrp->pci_cb_iobase0) {
			base = PCI_CB_IOBASE(pcrp->pci_cb_iobase0);
			limit = PCI_CB_IOLIMIT(pcrp->pci_cb_iolimit0);

			/*
			 * Deal with bridge ISA mode (256-wide ranges spaced 1K
			 * apart (start to start), but only in the first 64K).
			 */
			if (pcrp->pci_bridge_control & PCI_PCI_BRIDGE_ISA_EN) {
			    while ((base <= (CARD16)(-1)) &&
				   (base <= limit)) {
				if ((base + (CARD8)(-1)) <= limit) {
				    PCI_I_RANGE(range, pcrp->tag,
						base, base + (CARD8)(-1),
						ResIo | ResBlock |
						 ResExclusive);
				    PciBus->preferred_io =
					xf86AddResToList(PciBus->preferred_io,
							 &range, -1);
				    base += 0x0400;
				} else {
				    PCI_I_RANGE(range, pcrp->tag,
						base, limit,
						ResIo | ResBlock |
						 ResExclusive);
				    PciBus->preferred_io =
					xf86AddResToList(PciBus->preferred_io,
							 &range, -1);
				    base = limit + 1;
				}
			    }
			}

			if (base <= limit) {
			    PCI_I_RANGE(range, pcrp->tag, base, limit,
					ResIo | ResBlock | ResExclusive);
			    PciBus->preferred_io =
				xf86AddResToList(PciBus->preferred_io,
						 &range, -1);
			}
		    }

		    if (pcrp->pci_cb_iobase1) {
			base = PCI_CB_IOBASE(pcrp->pci_cb_iobase1);
			limit = PCI_CB_IOLIMIT(pcrp->pci_cb_iolimit1);

			/*
			 * Deal with bridge ISA mode (256-wide ranges spaced 1K
			 * apart (start to start), but only in the first 64K).
			 */
			if (pcrp->pci_bridge_control & PCI_PCI_BRIDGE_ISA_EN) {
			    while ((base <= (CARD16)(-1)) &&
				   (base <= limit)) {
				if ((base + (CARD8)(-1)) <= limit) {
				    PCI_I_RANGE(range, pcrp->tag,
						base, base + (CARD8)(-1),
						ResIo | ResBlock |
						 ResExclusive);
				    PciBus->preferred_io =
					xf86AddResToList(PciBus->preferred_io,
							 &range, -1);
				    base += 0x0400;
				} else {
				    PCI_I_RANGE(range, pcrp->tag,
						base, limit,
						ResIo | ResBlock |
						 ResExclusive);
				    PciBus->preferred_io =
					xf86AddResToList(PciBus->preferred_io,
							 &range, -1);
				    base = limit + 1;
				}
			    }
			}

			if (base <= limit) {
			    PCI_I_RANGE(range, pcrp->tag, base, limit,
					ResIo | ResBlock | ResExclusive);
			    PciBus->preferred_io =
				xf86AddResToList(PciBus->preferred_io,
						 &range, -1);
			}
		    }
		}

		if (pcrp->pci_command & PCI_CMD_MEM_ENABLE) {
		    if ((pcrp->pci_cb_membase0) &&
			(pcrp->pci_cb_membase0 <= pcrp->pci_cb_memlimit0)) {
			PCI_M_RANGE(range, pcrp->tag,
				    pcrp->pci_cb_membase0 & ~0x0fff,
				    pcrp->pci_cb_memlimit0 | 0x0fff,
				    ResMem | ResBlock | ResExclusive);
			if (pcrp->pci_bridge_control &
			    PCI_CB_BRIDGE_CTL_PREFETCH_MEM0)
			    PciBus->preferred_pmem =
				xf86AddResToList(PciBus->preferred_pmem,
						 &range, -1);
			else
			    PciBus->preferred_mem =
				xf86AddResToList(PciBus->preferred_mem,
						 &range, -1);
		    }
		    if ((pcrp->pci_cb_membase1) &&
			(pcrp->pci_cb_membase1 <= pcrp->pci_cb_memlimit1)) {
			PCI_M_RANGE(range, pcrp->tag,
				    pcrp->pci_cb_membase1 & ~0x0fff,
				    pcrp->pci_cb_memlimit1 | 0x0fff,
				    ResMem | ResBlock | ResExclusive);
			if (pcrp->pci_bridge_control &
			    PCI_CB_BRIDGE_CTL_PREFETCH_MEM1)
			    PciBus->preferred_pmem =
				xf86AddResToList(PciBus->preferred_pmem,
						 &range, -1);
			else
			    PciBus->preferred_mem =
				xf86AddResToList(PciBus->preferred_mem,
						 &range, -1);
		    }
		}

		break;

	    case PCI_SUBCLASS_BRIDGE_ISA:
	    case PCI_SUBCLASS_BRIDGE_EISA:
	    case PCI_SUBCLASS_BRIDGE_MC:
		*pnPciBus = PciBus = xnfcalloc(1, sizeof(PciBusRec));
		pnPciBus = &PciBus->next;
		PciBus->domain = domain;
		PciBus->primary = pcrp->busnum;
		PciBus->secondary = PciBus->subordinate = -1;
		PciBus->brbus = pcrp->busnum;
		PciBus->brdev = pcrp->devnum;
		PciBus->brfunc = pcrp->funcnum;
		PciBus->subclass = sub_class;
		PciBus->brcontrol = PCI_PCI_BRIDGE_VGA_EN;
		break;

	    case PCI_SUBCLASS_BRIDGE_HOST:
		/* Is this the correct bridge?  If not, ignore bus info */
		pBusInfo = pcrp->businfo;
		if (pBusInfo == HOST_NO_BUS)
		    break;

		secondary = 0;
		if (pBusInfo) {
		    /* Find "secondary" bus segment */
		    while (pBusInfo != pciBusInfo[secondary])
			secondary++;
		    if (pcrp != pBusInfo->bridge) {
			xf86MsgVerb(X_WARNING, 3, "Host bridge mismatch for"
				    " bus %x: %x:%x:%x and %x:%x:%x\n",
				    pBusInfo->primary_bus,
				    pcrp->busnum, pcrp->devnum, pcrp->funcnum,
				    pBusInfo->bridge->busnum,
				    pBusInfo->bridge->devnum,
				    pBusInfo->bridge->funcnum);
			pBusInfo = NULL;
		    }
		}

		*pnPciBus = PciBus = xnfcalloc(1, sizeof(PciBusRec));
		pnPciBus = &PciBus->next;

		PciBus->domain = domain;
		PciBus->primary = -1;
		PciBus->secondary = -1; /* to be set below */
		PciBus->subordinate = pciNumBuses - 1;

		if (pBusInfo) {
		    PciBus->primary = PciBus->secondary = secondary;
		    if (pBusInfo->funcs->pciGetBridgeBuses)
			(*pBusInfo->funcs->pciGetBridgeBuses)
			    (secondary,
			     &PciBus->primary,
			     &PciBus->secondary,
			     &PciBus->subordinate);
		}

		PciBus->brbus = pcrp->busnum;
		PciBus->brdev = pcrp->devnum;
		PciBus->brfunc = pcrp->funcnum;

		PciBus->subclass = sub_class;

		if (pBusInfo && pBusInfo->funcs->pciControlBridge)
		    PciBus->brcontrol =
			(*pBusInfo->funcs->pciControlBridge)(secondary, 0, 0);
		else
		    PciBus->brcontrol = PCI_PCI_BRIDGE_VGA_EN;

		if (pBusInfo && pBusInfo->funcs->pciGetBridgeResources) {
		    (*pBusInfo->funcs->pciGetBridgeResources)
			(secondary,
			 (pointer *)&PciBus->preferred_io,
			 (pointer *)&PciBus->preferred_mem,
			 (pointer *)&PciBus->preferred_pmem);
		    break;
		}

		PciBus->preferred_io =
		    xf86ExtractTypeFromList(pciBusAccWindows,
					    RANGE_TYPE(ResIo, domain));
		PciBus->preferred_mem =
		    xf86ExtractTypeFromList(pciBusAccWindows,
					    RANGE_TYPE(ResMem, domain));
		PciBus->preferred_pmem =
		    xf86ExtractTypeFromList(pciBusAccWindows,
					    RANGE_TYPE(ResMem, domain));
		break;

	    default:
		break;
	    }
	}
    }
    for (i = 0; i <= MaxBus; i++) { /* find PCI buses not attached to bridge */
	if (!pciBusInfo[i] || (pciBusInfo[i]->numDevices == 0))
	    continue;
	for (PciBus = PciBusBase; PciBus; PciBus = PciBus->next)
	    if (PciBus->secondary == i) break;
	if (!PciBus) {  /* We assume it's behind a HOST-PCI bridge */
	    /*
	     * Find the 'smallest' free HOST-PCI bridge, where 'small' is in
	     * the order of pciTag().
	     */
	    PCITAG minTag = (PCITAG)(-1L), tag;
	    PciBusPtr PciBusFound = NULL;
	    for (PciBus = PciBusBase; PciBus; PciBus = PciBus->next)
		if ((PciBus->subclass == PCI_SUBCLASS_BRIDGE_HOST) &&
		    (PciBus->secondary == -1) &&
		    (PciBus->brbus == i) &&
		    ((tag = pciTag(PciBus->brbus,PciBus->brdev,PciBus->brfunc))
		     < minTag) )  {
		    minTag = tag;
		    PciBusFound = PciBus;
		}
	    if (PciBusFound)
		PciBusFound->secondary = i;
	    else {  /* if nothing found it may not be visible: create new */
		/* Find a device on this bus */
		domain = 0;
		for (pcrpp = xf86PciInfo;  (pcrp = *pcrpp);  pcrpp++) {
		    if (pcrp->busnum == i) {
			domain = xf86GetPciDomain(pcrp->tag);
			break;
		    }
		}
		*pnPciBus = PciBus = xnfcalloc(1, sizeof(PciBusRec));
		pnPciBus = &PciBus->next;
		PciBus->domain = domain;
		PciBus->primary = PciBus->secondary = i;
		PciBus->subclass = PCI_SUBCLASS_BRIDGE_HOST;
		PciBus->brcontrol = PCI_PCI_BRIDGE_VGA_EN;
		PciBus->preferred_io =
		    xf86ExtractTypeFromList(pciBusAccWindows,
					    RANGE_TYPE(ResIo, domain));
		PciBus->preferred_mem =
		    xf86ExtractTypeFromList(pciBusAccWindows,
					    RANGE_TYPE(ResMem, domain));
		PciBus->preferred_pmem =
		    xf86ExtractTypeFromList(pciBusAccWindows,
					    RANGE_TYPE(ResMem, domain));
	    }
	}
    }

    for (PciBus = PciBusBase; PciBus; PciBus = PciBus->next) {
	if (PciBus->primary == PciBus->secondary) {
	    alignBridgeRanges(PciBusBase, PciBus);
	}
    }

    for (PciBus = PciBusBase; PciBus; PciBus = PciBus->next) {
	switch (PciBus->subclass) {
	    case PCI_SUBCLASS_BRIDGE_PCI:
		if (PciBus->interface == PCI_IF_BRIDGE_PCI_SUBTRACTIVE)
		    xf86MsgVerb(X_INFO, 3, "Subtractive PCI-to-PCI bridge:\n");
		else
		    xf86MsgVerb(X_INFO, 3, "PCI-to-PCI bridge:\n");
		break;
	    case PCI_SUBCLASS_BRIDGE_CARDBUS:
		xf86MsgVerb(X_INFO, 3, "PCI-to-CardBus bridge:\n");
		break;
	    case PCI_SUBCLASS_BRIDGE_HOST:
		xf86MsgVerb(X_INFO, 3, "Host-to-PCI bridge:\n");
		break;
	    case PCI_SUBCLASS_BRIDGE_ISA:
		xf86MsgVerb(X_INFO, 3, "PCI-to-ISA bridge:\n");
		break;
	    case PCI_SUBCLASS_BRIDGE_EISA:
		xf86MsgVerb(X_INFO, 3, "PCI-to-EISA bridge:\n");
		break;
	    case PCI_SUBCLASS_BRIDGE_MC:
		xf86MsgVerb(X_INFO, 3, "PCI-to-MCA bridge:\n");
		break;
	    default:
		break;
	}
	printBridgeInfo(PciBus);
    }
    xf86FreeResList(pciBusAccWindows);
    return PciBusBase;
}

static void
alignBridgeRanges(PciBusPtr PciBusBase, PciBusPtr primary)
{
    PciBusPtr PciBus;

    for (PciBus = PciBusBase; PciBus; PciBus = PciBus->next) {
	if ((PciBus != primary) && (PciBus->primary != -1)
	    && (PciBus->primary == primary->secondary)) {
	    resPtr tmp;
	    tmp = xf86FindIntersectOfLists(primary->preferred_io,
					   PciBus->preferred_io);
	    xf86FreeResList(PciBus->preferred_io);
	    PciBus->preferred_io = tmp;
	    tmp = xf86FindIntersectOfLists(primary->preferred_pmem,
					   PciBus->preferred_pmem);
	    xf86FreeResList(PciBus->preferred_pmem);
	    PciBus->preferred_pmem = tmp;
	    tmp = xf86FindIntersectOfLists(primary->preferred_mem,
					   PciBus->preferred_mem);
	    xf86FreeResList(PciBus->preferred_mem);
	    PciBus->preferred_mem = tmp;

	    /* Deal with subtractive decoding */
	    switch (PciBus->subclass) {
	    case PCI_SUBCLASS_BRIDGE_PCI:
		if (PciBus->interface != PCI_IF_BRIDGE_PCI_SUBTRACTIVE)
		    break;
		/* Fall through */
#if 0	/* Not yet */
	    case PCI_SUBCLASS_BRIDGE_ISA:
	    case PCI_SUBCLASS_BRIDGE_EISA:
	    case PCI_SUBCLASS_BRIDGE_MC:
#endif
		if (!(PciBus->io = primary->io))
		    PciBus->io = primary->preferred_io;
		if (!(PciBus->mem = primary->mem))
		    PciBus->mem = primary->preferred_mem;
		if (!(PciBus->pmem = primary->pmem))
		    PciBus->pmem = primary->preferred_pmem;
	    default:
		break;
	    }

	    alignBridgeRanges(PciBusBase, PciBus);
	}
    }
}

void
ValidatePci(void)
{
    pciVideoPtr pvp, pvp1;
    PciBusPtr pbp;
    pciConfigPtr pcrp, *pcrpp;
    CARD32 *basep;
    resPtr Sys;
    resRange range;
    int n = 0, m, i;

    if (!xf86PciVideoInfo) return;

    /*
     * Mark all pciInfoRecs that need to be validated. These are
     * the ones which have been assigned to a screen.
     */
    Sys = xf86DupResList(osRes);
    /* Only validate graphics devices in use */
    for (i=0; i<xf86NumScreens; i++) {
	for (m = 0; m < xf86Screens[i]->numEntities; m++)
	    if ((pvp = xf86GetPciInfoForEntity(xf86Screens[i]->entityList[m])))
		pvp->validate = TRUE;
    }

    /*
     * Collect all background PCI resources we need to validate against.
     * These are all resources which don't belong to PCINONSYSTEMCLASSES
     * and which have not been assigned to an entity.
     */
    /* First get the PCIINFOCLASSES */
    m = 0;
    while ((pvp = xf86PciVideoInfo[m++])) {
	/* is it a PCINONSYSTEMCLASS? */
	if (PCINONSYSTEMCLASSES(pvp->class, pvp->subclass))
	    continue;
	/* has it an Entity assigned to it? */
	for (i = 0; i < xf86NumEntities; i++) {
	    EntityPtr p = xf86Entities[i];
	    if (p->busType != BUS_PCI)
		continue;
	    if (p->pciBusId.bus == pvp->bus
		&& p->pciBusId.device == pvp->device
		&& p->pciBusId.func == pvp->func)
		break;
	}
	if (i != xf86NumEntities) /* found an Entity for this one */
	    continue;

	for (i = 0; i<6; i++) {
	    if (pvp->ioBase[i]) {
		PV_I_RANGE(range,pvp,i,ResExcIoBlock);
		Sys = xf86AddResToList(Sys,&range,-1);
	    } else if (pvp->memBase[i]) {
		PV_M_RANGE(range,pvp,i,ResExcMemBlock);
		Sys = xf86AddResToList(Sys,&range,-1);
	    }
	}
    }
    for (pcrpp = xf86PciInfo, pcrp = *pcrpp; pcrp; pcrp = *++(pcrpp)) {
	CARD8 header_type, numbars;

	/* These were handled above */
	if (PCIINFOCLASSES(pcrp->pci_base_class, pcrp->pci_sub_class))
	    continue;

	if (!(pcrp->pci_command & (PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE)))
	    continue;

	header_type = pcrp->pci_header_type & 0x7f;
	switch (header_type) {
	case 0:
	    numbars = 6;
	    break;

	case 1:
	    numbars = 2;
	    break;

	case 2:
	    numbars = 1;
	    break;

	default:
	    continue;
	}

	basep = &pcrp->pci_base0;
	for (i = 0; i < numbars; i++) {
	    if (pcrp->basesize[i]) {
		if (PCI_MAP_IS_IO(basep[i]))  {
		    if (!(pcrp->pci_command & PCI_CMD_IO_ENABLE))
			continue;
		    P_I_RANGE(range, pcrp->tag, PCIGETIO(basep[i]),
			      pcrp->basesize[i], ResExcIoBlock)
		} else if (!PCI_MAP_IS64BITMEM(basep[i])) {
		    if (!(pcrp->pci_command & PCI_CMD_MEM_ENABLE))
			continue;
		    P_M_RANGE(range, pcrp->tag, PCIGETMEMORY(basep[i]),
			      pcrp->basesize[i], ResExcMemBlock)
		} else {
		    i++;
		    if (i >= numbars)
			break;
		    if (!(pcrp->pci_command & PCI_CMD_MEM_ENABLE))
			continue;
#if defined(LONG64) || defined(WORD64)
		    P_M_RANGE(range, pcrp->tag, PCIGETMEMORY64(basep[i-1]),
			      pcrp->basesize[i-1], ResExcMemBlock)
#else
		    if (basep[i])
			continue;
		    P_M_RANGE(range, pcrp->tag, PCIGETMEMORY(basep[i-1]),
			      pcrp->basesize[i-1], ResExcMemBlock)
#endif
		}
		Sys = xf86AddResToList(Sys, &range, -1);
	    }
	}

	if ((pcrp->pci_command & PCI_CMD_MEM_ENABLE) &&
	    (pcrp->basesize[6] > 0)) {
	    switch (header_type) {
	    case 0:
		if (!(pcrp->pci_baserom & PCI_MAP_ROM_DECODE_ENABLE))
		    break;
		P_M_RANGE(range, pcrp->tag, PCIGETROM(pcrp->pci_baserom),
			  pcrp->basesize[6], ResExcMemBlock);
		Sys = xf86AddResToList(Sys, &range, -1);
		break;

	    case 1:
		if (!(pcrp->pci_br_rom & PCI_MAP_ROM_DECODE_ENABLE))
		    break;
		P_M_RANGE(range, pcrp->tag, PCIGETROM(pcrp->pci_br_rom),
			  pcrp->basesize[6], ResExcMemBlock);
		Sys = xf86AddResToList(Sys, &range, -1);
		break;

	    default:
		break;
	    }
	}
    }

#ifdef DEBUG
    xf86MsgVerb(X_INFO, 3,"Sys:\n");
    xf86PrintResList(3,Sys);
#endif

    /*
     * The order the video devices are listed in is
     * just right: the lower buses come first.
     * This way we attempt to fix a conflict of
     * a lower bus device with a higher bus device
     * where we have more room to find different
     * resources.
     */
    while ((pvp = xf86PciVideoInfo[n++])) {
	resPtr res_mp = NULL, res_m_io = NULL;
	resPtr NonSys;
	resPtr tmp, avoid = NULL;

	if (!pvp->validate) continue;
	NonSys = xf86DupResList(Sys);
	m = n;
	while ((pvp1 = xf86PciVideoInfo[m++])) {
	    if (!pvp1->validate) continue;
	    pcrp = pvp1->thisCard;
	    basep = &pcrp->pci_base0;
	    for (i = 0; i<6; i++) {
		if (pvp1->size[i] <= 0) continue;
		if (PCI_MAP_IS_IO(basep[i])) {
		    PV_I_RANGE(range,pvp1,i,ResExcIoBlock);
		    NonSys = xf86AddResToList(NonSys,&range,-1);
		} else {
		    PV_M_RANGE(range,pvp1,i,ResExcMemBlock);
		    NonSys = xf86AddResToList(NonSys,&range,-1);
		}
	    }
	}
#ifdef DEBUG
	xf86MsgVerb(X_INFO, 3,"NonSys:\n");
	xf86PrintResList(3,NonSys);
#endif
	pbp = xf86PciBus;
	while (pbp) {
	    if (pbp->secondary == pvp->bus) {
		if (pbp->preferred_pmem) {
		    /* keep prefetchable separate */
		    res_mp =
			xf86FindIntersectOfLists(pbp->preferred_pmem, ResRange);
		}
		if (pbp->pmem) {
		    res_mp = xf86FindIntersectOfLists(pbp->pmem, ResRange);
		}
		if (pbp->preferred_mem) {
		    res_m_io =
			xf86FindIntersectOfLists(pbp->preferred_mem, ResRange);
		}
		if (pbp->mem) {
		    res_m_io = xf86FindIntersectOfLists(pbp->mem, ResRange);
		}
		if (pbp->preferred_io) {
		    res_m_io = xf86JoinResLists(res_m_io,
			xf86FindIntersectOfLists(pbp->preferred_io, ResRange));
		}
		if (pbp->io) {
		    res_m_io = xf86JoinResLists(res_m_io,
			xf86FindIntersectOfLists(pbp->preferred_io, ResRange));
		}
	    } else if ((pbp->primary == pvp->bus) &&
		       (pbp->secondary >= 0) &&
		       (pbp->primary != pbp->secondary)) {
		tmp = xf86DupResList(pbp->preferred_pmem);
		avoid = xf86JoinResLists(avoid, tmp);
		tmp = xf86DupResList(pbp->preferred_mem);
		avoid = xf86JoinResLists(avoid, tmp);
		tmp = xf86DupResList(pbp->preferred_io);
		avoid = xf86JoinResLists(avoid, tmp);
	    }
	    pbp = pbp->next;
	}
	if (res_m_io == NULL)
	   res_m_io = xf86DupResList(ResRange);

	pciConvertListToHost(pvp->bus,pvp->device,pvp->func, avoid);

#ifdef DEBUG
	xf86MsgVerb(X_INFO, 3,"avoid:\n");
	xf86PrintResList(3,avoid);
	xf86MsgVerb(X_INFO, 3,"prefetchable Memory:\n");
	xf86PrintResList(3,res_mp);
	xf86MsgVerb(X_INFO, 3,"MEM/IO:\n");
	xf86PrintResList(3,res_m_io);
#endif
	pcrp = pvp->thisCard;
	basep = &pcrp->pci_base0;
	for (i = 0; i < 6; i++) {
	    int j;
	    resPtr own;

	    if (pvp->size[i] <= 0) continue;
	    own = NULL;
	    for (j = i+1; j < 6; j++) {
		if (pvp->size[j] <= 0) continue;
		if (PCI_MAP_IS_IO(basep[j])) {
		    PV_I_RANGE(range,pvp,j,ResExcIoBlock);
		    own = xf86AddResToList(own,&range,-1);
		} else {
		    PV_M_RANGE(range,pvp,j,ResExcMemBlock);
		    own = xf86AddResToList(own,&range,-1);
		}
	    }
#ifdef DEBUG
	    xf86MsgVerb(X_INFO, 3, "own:\n");
	    xf86PrintResList(3, own);
#endif
	    if (PCI_MAP_IS_IO(basep[i])) {
		PV_I_RANGE(range,pvp,i,ResExcIoBlock);
		if (xf86IsSubsetOf(range,res_m_io)
		    && ! ChkConflict(&range,own,SETUP)
		    && ! ChkConflict(&range,avoid,SETUP)
		    && ! ChkConflict(&range,NonSys,SETUP)) {
		    xf86FreeResList(own);
		    continue;
		}
		xf86MsgVerb(X_WARNING, 0,
			"****INVALID IO ALLOCATION**** b: 0x%lx e: 0x%lx "
			"correcting\a\n", range.rBegin,range.rEnd);
#ifdef DEBUG
		sleep(2);
#endif
		fixPciResource(i, 0, pvp, range.type);
	    } else {
		PV_M_RANGE(range,pvp,i,ResExcMemBlock);
		if (pvp->type[i] & PCI_MAP_MEMORY_CACHABLE) {
		    if (xf86IsSubsetOf(range,res_mp)
			&& ! ChkConflict(&range,own,SETUP)
			&& ! ChkConflict(&range,avoid,SETUP)
			&& ! ChkConflict(&range,NonSys,SETUP)) {
			xf86FreeResList(own);
			continue;
		    }
		}
		if (xf86IsSubsetOf(range,res_m_io)
		    && ! ChkConflict(&range,own,SETUP)
		    && ! ChkConflict(&range,avoid,SETUP)
		    && ! ChkConflict(&range,NonSys,SETUP)) {
		    xf86FreeResList(own);
		    continue;
		}
		xf86MsgVerb(X_WARNING, 0,
			"****INVALID MEM ALLOCATION**** b: 0x%lx e: 0x%lx "
			"correcting\a\n", range.rBegin,range.rEnd);
#ifdef DEBUG
		sleep(2);
#endif
		fixPciResource(i, 0, pvp, range.type);
	    }
	    xf86FreeResList(own);
	}
	xf86FreeResList(avoid);
	xf86FreeResList(NonSys);
	xf86FreeResList(res_mp);
	xf86FreeResList(res_m_io);
    }
    xf86FreeResList(Sys);
}

resList
GetImplicitPciResources(int entityIndex)
{
    pciVideoPtr pvp;
    int i;
    resList list = NULL;
    int num = 0;

    if (! (pvp = xf86GetPciInfoForEntity(entityIndex))) return NULL;

    for (i = 0; i < 6; i++) {
	if (pvp->ioBase[i]) {
	    list = xnfrealloc(list,sizeof(resRange) * (++num));
	    PV_I_RANGE(list[num - 1],pvp,i,ResShrIoBlock | ResBios);
	} else if (pvp->memBase[i]) {
	    list = xnfrealloc(list,sizeof(resRange) * (++num));
	    PV_M_RANGE(list[num - 1],pvp,i,ResShrMemBlock | ResBios);
	}
    }
#if 0
    if (pvp->biosBase) {
	list = xnfrealloc(list,sizeof(resRange) * (++num));
	PV_B_RANGE(list[num - 1],pvp,ResShrMemBlock | ResBios);
    }
#endif
    list = xnfrealloc(list,sizeof(resRange) * (++num));
    list[num - 1].type = ResEnd;

    return list;
}

void
initPciState(void)
{
    int i = 0;
    int j = 0;
    pciVideoPtr pvp;
    pciAccPtr pcaccp;

    if (xf86PciAccInfo != NULL)
	return;

    if (xf86PciVideoInfo == NULL)
	return;

    while ((pvp = xf86PciVideoInfo[i]) != NULL) {
	i++;
	    j++;
	    xf86PciAccInfo = xnfrealloc(xf86PciAccInfo,
					sizeof(pciAccPtr) * (j + 1));
	    xf86PciAccInfo[j] = NULL;
	    pcaccp = xf86PciAccInfo[j - 1] = xnfalloc(sizeof(pciAccRec));
	    pcaccp->busnum = pvp->bus;
	    pcaccp->devnum = pvp->device;
	    pcaccp->funcnum = pvp->func;
	    pcaccp->arg.tag = pciTag(pvp->bus, pvp->device, pvp->func);
	    pcaccp->ioAccess.AccessDisable = pciIoAccessDisable;
	    pcaccp->ioAccess.AccessEnable = pciIoAccessEnable;
	    pcaccp->ioAccess.arg = &pcaccp->arg;
	    pcaccp->io_memAccess.AccessDisable = pciIo_MemAccessDisable;
	    pcaccp->io_memAccess.AccessEnable = pciIo_MemAccessEnable;
	    pcaccp->io_memAccess.arg = &pcaccp->arg;
	    pcaccp->memAccess.AccessDisable = pciMemAccessDisable;
	    pcaccp->memAccess.AccessEnable = pciMemAccessEnable;
	    pcaccp->memAccess.arg = &pcaccp->arg;
	    if (PCISHAREDIOCLASSES(pvp->class, pvp->subclass))
		pcaccp->ctrl = TRUE;
	    else
		pcaccp->ctrl = FALSE;
	    savePciState(pcaccp->arg.tag, &pcaccp->save);
	    pcaccp->arg.ctrl = pcaccp->save.command;
    }
}

/*
 * initPciBusState() - fill out the BusAccRec for a PCI bus.
 * Theory: each bus is associated with one bridge connecting it
 * to its parent bus. The address of a bridge is therefore stored
 * in the BusAccRec of the bus it connects to. Each bus can
 * have several bridges connecting secondary buses to it. Only one
 * of these bridges can be open. Therefore the status of a bridge
 * associated with a bus is stored in the BusAccRec of the parent
 * the bridge connects to. The first member of the structure is
 * a pointer to a function that open access to this bus. This function
 * receives a pointer to the structure itself as argument. This
 * design should be common to BusAccRecs of any type of buses we
 * support. The remeinder of the structure is bus type specific.
 * In this case it contains a pointer to the structure of the
 * parent bus. Thus enabling access to a specific bus is simple:
 * 1. Close any bridge going to secondary buses.
 * 2. Climb down the ladder and enable any bridge on buses
 *    on the path from the CPU to this bus.
 */

void
initPciBusState(void)
{
    BusAccPtr pbap, pbap_tmp;
    PciBusPtr pbp = xf86PciBus;
    pciBusInfo_t *pBusInfo;

    while (pbp) {
	pbap = xnfcalloc(1,sizeof(BusAccRec));
	pbap->busdep.pci.bus = pbp->secondary;
	pbap->busdep.pci.primary_bus = pbp->primary;
	pbap->busdep_type = BUS_PCI;
	pbap->busdep.pci.acc = PCITAG_SPECIAL;

	if ((pbp->secondary >= 0) && (pbp->secondary < pciNumBuses) &&
	    (pBusInfo = pciBusInfo[pbp->secondary]) &&
	    pBusInfo->funcs->pciControlBridge) {
	    pbap->type = BUS_PCI;
	    pbap->save_f = savePciDrvBusState;
	    pbap->restore_f = restorePciDrvBusState;
	    pbap->set_f = pciSetBusAccess;
	    pbap->enable_f = pciDrvBusAccessEnable;
	    pbap->disable_f = pciDrvBusAccessDisable;
	    savePciDrvBusState(pbap);
	} else switch (pbp->subclass) {
	case PCI_SUBCLASS_BRIDGE_HOST:
	    pbap->type = BUS_PCI;
	    pbap->set_f = pciSetBusAccess;
	    break;
	case PCI_SUBCLASS_BRIDGE_PCI:
	case PCI_SUBCLASS_BRIDGE_CARDBUS:
	    pbap->type = BUS_PCI;
	    pbap->save_f = savePciBusState;
	    pbap->restore_f = restorePciBusState;
	    pbap->set_f = pciSetBusAccess;
	    pbap->enable_f = pciBusAccessEnable;
	    pbap->disable_f = pciBusAccessDisable;
	    pbap->busdep.pci.acc = pciTag(pbp->brbus,pbp->brdev,pbp->brfunc);
	    savePciBusState(pbap);
	    break;
	case PCI_SUBCLASS_BRIDGE_ISA:
	case PCI_SUBCLASS_BRIDGE_EISA:
	case PCI_SUBCLASS_BRIDGE_MC:
	    pbap->type = BUS_ISA;
	    pbap->set_f = pciSetBusAccess;
	    break;
	}
	pbap->next = xf86BusAccInfo;
	xf86BusAccInfo = pbap;
	pbp = pbp->next;
    }

    pbap = xf86BusAccInfo;

    while (pbap) {
	pbap->primary = NULL;
	if (pbap->busdep_type == BUS_PCI
	    && pbap->busdep.pci.primary_bus > -1) {
	    pbap_tmp = xf86BusAccInfo;
	    while (pbap_tmp) {
		if (pbap_tmp->busdep_type == BUS_PCI &&
		    pbap_tmp->busdep.pci.bus == pbap->busdep.pci.primary_bus) {
		    /* Don't create loops */
		    if (pbap == pbap_tmp)
			break;
		    pbap->primary = pbap_tmp;
		    break;
		}
		pbap_tmp = pbap_tmp->next;
	    }
	}
	pbap = pbap->next;
    }
}

void
PciStateEnter(void)
{
    pciAccPtr paccp;
    int i = 0;

    if (xf86PciAccInfo == NULL)
	return;

    while ((paccp = xf86PciAccInfo[i]) != NULL) {
	i++;
	if (!paccp->ctrl)
	    continue;
	savePciState(paccp->arg.tag, &paccp->save);
	restorePciState(paccp->arg.tag, &paccp->restore);
	paccp->arg.ctrl = paccp->restore.command;
    }
}

void
PciBusStateEnter(void)
{
    BusAccPtr pbap = xf86BusAccInfo;

    while (pbap) {
	if (pbap->save_f)
	    pbap->save_f(pbap);
	pbap = pbap->next;
    }
}

void
PciStateLeave(void)
{
    pciAccPtr paccp;
    int i = 0;

    if (xf86PciAccInfo == NULL)
	return;

    while ((paccp = xf86PciAccInfo[i]) != NULL) {
	i++;
	if (!paccp->ctrl)
	    continue;
	savePciState(paccp->arg.tag, &paccp->restore);
	restorePciState(paccp->arg.tag, &paccp->save);
    }
}

void
PciBusStateLeave(void)
{
    BusAccPtr pbap = xf86BusAccInfo;

    while (pbap) {
	if (pbap->restore_f)
	    pbap->restore_f(pbap);
	pbap = pbap->next;
    }
}

void
DisablePciAccess(void)
{
    int i = 0;
    pciAccPtr paccp;
    if (xf86PciAccInfo == NULL)
	return;

    while ((paccp = xf86PciAccInfo[i]) != NULL) {
	i++;
	if (!paccp->ctrl) /* disable devices that are under control initially*/
	    continue;
	pciIo_MemAccessDisable(paccp->io_memAccess.arg);
    }
}

void
DisablePciBusAccess(void)
{
    BusAccPtr pbap = xf86BusAccInfo;

    while (pbap) {
	if (pbap->disable_f)
	    pbap->disable_f(pbap);
	if (pbap->primary)
	    pbap->primary->current = NULL;
	pbap = pbap->next;
    }
}

/*
 * Public functions
 */

Bool
xf86IsPciDevPresent(int bus, int dev, int func)
{
    int i = 0;
    pciConfigPtr pcp;

    while ((pcp = xf86PciInfo[i]) != NULL) {
	if ((pcp->busnum == bus)
	    && (pcp->devnum == dev)
	    && (pcp->funcnum == func))
	    return TRUE;
	i++;
    }
    return FALSE;
}

/*
 * If the slot requested is already in use, return -1.
 * Otherwise, claim the slot for the screen requesting it.
 */

int
xf86ClaimPciSlot(int bus, int device, int func, DriverPtr drvp,
		 int chipset, GDevPtr dev, Bool active)
{
    EntityPtr p = NULL;
    pciAccPtr *ppaccp = xf86PciAccInfo;
    BusAccPtr pbap = xf86BusAccInfo;

    int num;

    if (xf86CheckPciSlot(bus, device, func)) {
	if ((num = FindDiscardedPciSlot(bus, device, func)) < 0)
	    num = xf86AllocateEntity();
	p = xf86Entities[num];
	p->driver = drvp;
	p->chipset = chipset;
	p->busType = BUS_PCI;
	p->pciBusId.bus = bus;
	p->pciBusId.device = device;
	p->pciBusId.func = func;
	p->active = active;
	p->inUse = FALSE;
	if (dev)
	    xf86AddDevToEntity(num, dev);
	/* Here we initialize the access structure */
	p->access = xnfcalloc(1,sizeof(EntityAccessRec));
	while (ppaccp && *ppaccp) {
	    if ((*ppaccp)->busnum == bus
		&& (*ppaccp)->devnum == device
		&& (*ppaccp)->funcnum == func) {
		p->access->fallback = &(*ppaccp)->io_memAccess;
		p->access->pAccess = &(*ppaccp)->io_memAccess;
		(*ppaccp)->ctrl = TRUE; /* mark control if not already */
		break;
	    }
	    ppaccp++;
	}
	if (!ppaccp || !*ppaccp) {
	    p->access->fallback = &AccessNULL;
	    p->access->pAccess = &AccessNULL;
	}

	p->busAcc = NULL;
	while (pbap) {
	    if (pbap->type == BUS_PCI && pbap->busdep.pci.bus == bus)
		p->busAcc = pbap;
	    pbap = pbap->next;
	}
	fixPciSizeInfo(num);

	/* in case bios is enabled disable it */
	disablePciBios(pciTag(bus,device,func));
	pciSlotClaimed = TRUE;

	if (active) {
	    /* Map in this domain's I/O space */
	   p->domainIO = xf86MapDomainIO(-1, VIDMEM_MMIO,
					 pciTag(bus, device, func), 0, 1);
	}

	return num;
    } else
	return -1;
}

/*
 * Get xf86PciVideoInfo for a driver.
 */
pciVideoPtr *
xf86GetPciVideoInfo(void)
{
    return xf86PciVideoInfo;
}

/* --- Used by ATI driver, but also more generally useful */

/*
 * Get the full xf86scanpci data.
 */
pciConfigPtr *
xf86GetPciConfigInfo(void)
{
    return xf86PciInfo;
}

/*
 * Enable a device and route VGA to it.  This is intended for a driver's
 * Probe(), before creating EntityRec's.  Only one device can be thus enabled
 * at any one time, and should be disabled when the driver is done with it.
 *
 * The following special calls are also available:
 *
 * pvp == NULL && rt == NONE    disable previously enabled device
 * pvp != NULL && rt == NONE    ensure device is disabled
 * pvp == NULL && rt != NONE    disable >all< subsequent calls to this function
 *                              (done from xf86PostProbe())
 * The last combination has been removed! To do this cleanly we have
 * to implement stages and need to test at each stage dependent function
 * if it is allowed to execute.
 *
 * The device represented by pvp may not have been previously claimed.
 */
void
xf86SetPciVideo(pciVideoPtr pvp, resType rt)
{
    static BusAccPtr pbap = NULL;
    static xf86AccessPtr pAcc = NULL;
    static Bool DoneProbes = FALSE;
    pciAccPtr pcaccp;
    int i;

    if (DoneProbes)
	return;

    /* Disable previous access */
    if (pAcc) {
	if (pAcc->AccessDisable)
	    (*pAcc->AccessDisable)(pAcc->arg);
	pAcc = NULL;
    }
    if (pbap) {
	while (pbap->primary) {
	    if (pbap->disable_f)
		(*pbap->disable_f)(pbap);
	    pbap->primary->current = NULL;
	    pbap = pbap->primary;
	}
	pbap = NULL;
    }

    /* Check for xf86PostProbe's magic combo */
    if (!pvp) {
	if (rt != NONE)
	    DoneProbes = TRUE;
	return;
    }

    /* Validate device */
    if (!xf86PciVideoInfo || !xf86PciAccInfo || !xf86BusAccInfo)
	return;

    for (i = 0; pvp != xf86PciVideoInfo[i]; i++)
	if (!xf86PciVideoInfo[i])
	    return;

    /* Ignore request for claimed adapters */
    if (!xf86CheckPciSlot(pvp->bus, pvp->device, pvp->func))
	return;

    /* Find pciAccRec structure */
    for (i = 0; ; i++) {
	if (!(pcaccp = xf86PciAccInfo[i]))
	    return;
	if ((pvp->bus == pcaccp->busnum) &&
	    (pvp->device == pcaccp->devnum) &&
	    (pvp->func == pcaccp->funcnum))
	    break;
    }

    if (rt == NONE) {
	/* This is a call to ensure the adapter is disabled */
	if (pcaccp->io_memAccess.AccessDisable)
	    (*pcaccp->io_memAccess.AccessDisable)(pcaccp->io_memAccess.arg);
	return;
    }

    /* Find BusAccRec structure */
    for (pbap = xf86BusAccInfo; ; pbap = pbap->next) {
	if (!pbap)
	    return;
	if (pvp->bus == pbap->busdep.pci.bus)
	    break;
    }

    /* Route VGA */
    if (pbap->set_f)
	(*pbap->set_f)(pbap);

    /* Enable device */
    switch (rt) {
    case IO:
	pAcc = &pcaccp->ioAccess;
	break;
    case MEM_IO:
	pAcc = &pcaccp->io_memAccess;
	break;
    case MEM:
	pAcc = &pcaccp->memAccess;
	break;
    default:	/* no compiler noise */
	break;
    }

    if (pAcc && pAcc->AccessEnable)
	(*pAcc->AccessEnable)(pAcc->arg);
}

/*
 * This function is called to determine whether or not a hard-failed master
 * abort can occur when testing if an adapter can actually be disabled through
 * PCI.  The function returns whether the adapter can be probed at all, and (in
 * *pActivate) whether enabling the adapter through PCI is a likely requisite
 * to prevent such hard-fails.  The function assumes that all resources the
 * adapter registers are actually routed to it through the bus tree.
 */
Bool
xf86CheckPciVideo(pciVideoPtr pvp, Bool *pActivate)
{
    PciBusPtr pBus;

    /* Start by assuming the adapter doesn't need to be enabled */
    if (pActivate)
	*pActivate = FALSE;

    /*
     * If no PCI bus segments are present, assume all master aborts complete
     * normally.  Note that no bus segments will be known when PCI scans are
     * disabled entirely, but that option should only be used when the presence
     * of PCI in a system is incorrectly detected.
     */
    if (!xf86PciBus)
	return TRUE;

    if (!pvp)	/* This one is actually a programming error */
	return FALSE;

    /* Find the adapter's bus segment structure */
    for (pBus = xf86PciBus;  ;  pBus = pBus->next) {
	if (!pBus)
	    return FALSE;

	if (pBus->secondary == pvp->bus)
	    break;
    }

    /* After this point, we will always return TRUE */
    if (!pActivate)
	return TRUE;

    /* Given we disable hard-fails on non-root bus segments... */
    if ((pBus->primary != -1) && (pBus->primary != pvp->bus))
	return TRUE;

    /*
     * Look for a subtractive decoder on the same segment as the adapter, on
     * the assumption that such bridges never generate master aborts on the
     * root segment.
     */
    for (pBus = xf86PciBus;  pBus;  pBus = pBus->next) {
	if (pBus->primary != pvp->bus)
	    continue;

	switch (pBus->subclass) {
#if 0  /* ??? */
	case PCI_SUBCLASS_BRIDGE_PCI:
	    if (pBus->interface != PCI_IF_BRIDGE_PCI_SUBTRACTIVE)
		continue;
	    /* XXX Should check if bridge routes adapter's resources */
	    /* Fall through */
#endif
	case PCI_SUBCLASS_BRIDGE_ISA:
	case PCI_SUBCLASS_BRIDGE_EISA:
	case PCI_SUBCLASS_BRIDGE_MC:
	    return TRUE;

	default:
	    continue;
	}
    }

    /*
     * At this point the adapter's compliance with PCI enablement/disablement
     * cannot be safely determined.
     */
    *pActivate = CAN_HARDFAIL_MASTER_ABORTS;

    return TRUE;
}

/*
 * A recursive helper for xf86CheckPciSparseIO() below.  This scans the PCI bus
 * sub-tree rooted at pBus1 for its sub-sub-tree to which a sparse resource
 * range is routed.  The function returns FALSE if more than one such
 * subordinate tree is found, or TRUE otherwise.  This sets *ppUnRouted to NULL
 * if exactly one subordinate is found.
 */
static Bool
xf86CheckPciSubTreeIO(PciBusPtr pBus1,
		      IOADDRESS Base, IOADDRESS Last, IOADDRESS End,
		      memType mask, Bool **ppUnrouted)
{
    PciBusPtr pBus2;
    resPtr pRes;
    IOADDRESS bot, top;
    PCITAG tag;
    resRange range;

    /*
     * Loop through all of of pBus1's secondary bus segments.  Note that these
     * include those behind a subtractive-decoding P2P bridge.
     */
    for (pBus2 = xf86PciBus;  pBus2;  pBus2 = pBus2->next) {
	if ((pBus1 == pBus2) || (pBus2->secondary == -1) ||
	    (pBus1->secondary != pBus2->primary))
	    continue;

	tag = pciTag(pBus2->secondary, 0, 0);

	/* Loop through all I/O resources routed to pBus2 */
	for (pRes = pBus2->preferred_io;  pRes;  pRes = pRes->next) {
	    /*
	     * Look for overlaps in the device (or bus) view, rather than in
	     * the host view, because the former is not as subject to OS
	     * mangling.  Assume that, for all "R" ...
	     *
	     *    (B2I(tag, I2B(tag, R)) == R) &&
	     *    (I2B(tag, B2I(tag, R)) == R)
	     *
	     * ... holds true.
	     */
	    bot = I2B(tag, pRes->block_begin);
	    top = I2B(tag, pRes->block_end);

	    if ((bot > End) || (top < Base))
		continue;	/* No potential overlap */

	    if ((bot > Base) || (top < End)) {
		/* A potential partial overlap */
		if (Base > bot)
		    bot = Base;

		if (End < top)
		    top = End;

		if ((bot & ~mask) == (top & ~mask)) {
		    if (((bot & mask) > Last) ||
			((top & mask) < Base))
			continue;	/* No overlap */
		}

		/*
		 * A partial overlap, but the bridge might route enough
		 * overlapping ranges to entirely cover the requested
		 * resources.  Use xf86IsSubsetOf() to determine this.
		 */
		bot = Base;
		while (bot <= Last) {
		    top = ((bot - 1) ^ bot) >> 1;
		    while ((bot + top) > Last)
			top >>= 1;

		    RANGE(range, B2ISB(tag, bot), B2ISM(tag, mask & ~top),
			  RANGE_TYPE(ResIo | ResSparse | ResExclusive,
				     pBus2->domain));

		    if (!xf86IsSubsetOf(range, pBus2->preferred_io))
			return FALSE;

		    bot += top + 1;
		}
	    }

	    /*
	     * pBus2 completely routes the requested resources.  Check its
	     * sub-trees.
	     */
	    if (!xf86CheckPciSubTreeIO(pBus2, Base, Last, End, mask,
				       ppUnrouted))
		return FALSE;

	    *ppUnrouted = NULL;
	    return TRUE;
	}
    }

    /* No bridge on pBus1 routes the resources */
    return TRUE;
}

/*
 * This function determines which secondary bridges, if any, would decode an
 * access to a range of sparse I/O addresses in a particular domain.  The
 * function returns FALSE if there is more than one such bridge in the system
 * (or some other error), and returns TRUE otherwise.  In the latter case, this
 * function also returns (in *pUnRouted) whether or not any such bridge exists.
 * This function is intended to be used by a driver before probing sparse I/O
 * addresses while all PCI display functions are disabled, to determine if such
 * a probe might cause a hard-failed master abort and crash the system.
 *
 * This function intentionally ignores VGA routing overrides, and, therefore,
 * should only be called while VGA routing is disabled throughout the system.
 *
 * The mask specified here, like all sparse masks throughout the server, must
 * be 1-extended to be portable to all host architectures.  But, at odds with
 * the rest of the server, the mask passed here applies to each _individual_
 * port in [Base, Base + count[, not to the range as a whole.  In particular,
 * should it ever be needed to run this function for a non-sparse range, simply
 * call it with an all-ones mask.
 */
Bool
xf86CheckPciSparseIO(int domain, IOADDRESS Base, int count, memType mask,
		     Bool *pUnRouted)
{
    IOADDRESS Last, End;
    PciBusPtr pBusRoot, pBus;
    int nRoot;

    /*
     * Begin with the assumption that the requested resources are routed away
     * from the root bus segment.
     */
    if (pUnRouted)
	*pUnRouted = FALSE;

    /*
     * If there are no PCI bus segments, assume all master aborts complete
     * normally.  PCI scans should only be disabled when the presence of PCI
     * is incorrectly detected.
     */
    if (!xf86PciBus)
	return TRUE;

    /* Sanity check */
    if (count < 1)
	return FALSE;

    /*
     * Find a root bus segment in the domain.  If none is found, the domain
     * isn't populated.  If more than one are present (a case dealt with below),
     * then we don't understand enough about the domain chipset to determine
     * how I/O routing is partitionned amongst these pseudo-root bus segments,
     * but we still look for a secondary that completely routes the requested
     * resources.
     */
    for (pBusRoot = xf86PciBus;  ;  pBusRoot = pBusRoot->next) {
	if (!pBusRoot)
	    return FALSE;

	if ((domain == pBusRoot->domain) && (pBusRoot->primary != -1) &&
	    (pBusRoot->primary == pBusRoot->secondary))
	    break;
    }

    /* Requested resources must be contiguous (within mask) */
    Last = Base;
    while (TRUE) {
	if ((Last & mask) != Last)
	    return FALSE;

	if (!--count)
	    break;

	Last++;
    }

    End = Last | ~mask;	/* Maximum I/O port of interest */
    nRoot = 0;		/* Number of "root" bus segments in domain */

    /* Loop through (and count) all root bus segments in the domain */
    for (pBus = pBusRoot;  pBus;  pBus = pBus->next) {
	if ((domain != pBus->domain) || (pBus->primary == -1) ||
	    (pBus->primary != pBus->secondary))
	    continue;

	nRoot++;

	if (!xf86CheckPciSubTreeIO(pBus, Base, Last, End, mask, &pUnRouted))
	    return FALSE;
    }

    /*
     * If the resources are routed to any secondary bus segment, or if the
     * caller doesn't care about the potential for hard-failed master aborts,
     * return now.
     */
    if (!pUnRouted)
	return TRUE;

    /*
     * If we know of only one root bus segment in the domain, look for a
     * pre-PCI subtractive decoder on it.  Otherwise, err on the side of
     * caution because we don't know enough about the domain chipset to
     * determine which pseudo-root bus segment would claim an access to the
     * requested resources.
     */
    if (nRoot == 1) {
	for (pBus = xf86PciBus;  pBus;  pBus = pBus->next) {
	    if ((pBus->primary == pBusRoot->secondary) &&
		(pBus->secondary == -1))
		return TRUE;
	}
    }

    /*
     * Notify caller that probing the requested resources might generate
     * hard-failed master aborts.
     */
    *pUnRouted = CAN_HARDFAIL_MASTER_ABORTS;

    return TRUE;
}

/*
 * This function determines whether a domain provides actual memory (ROM or
 * RAM) in the traditional x86 BIOS range (0x0C0000 to 0x0FFFF).  It currently
 * does so by looking for a pre-PCI bridge in the domain.
 */
Bool
xf86DomainHasBIOSSegments(int domain)
{
    PciBusPtr pBus;

    if (!xf86PciBus)
	return TRUE;

    for (pBus = xf86PciBus;  pBus;  pBus = pBus->next) {
	if ((domain == pBus->domain) && (pBus->primary != -1) &&
	    (pBus->secondary == -1))
	    return TRUE;
    }

    return !CAN_HARDFAIL_MASTER_ABORTS;
}

/*
 * Parse a BUS ID string, and return the PCI bus parameters if it was
 * in the correct format for a PCI bus id.
 */
Bool
xf86ParsePciBusString(const char *busID, int *bus, int *device, int *func)
{
    /*
     * The format is assumed to be "bus[@domain]:device[:func]", where domain,
     * bus, device and func are decimal integers.  domain and func may be
     * omitted and assumed to be zero, although doing this isn't encouraged.
     */

    char *p, *s, *d;
    const char *id;
    int i;

    if (StringToBusType(busID, &id) != BUS_PCI)
	return FALSE;

    s = xstrdup(id);
    p = strtok(s, ":");
    if (p == NULL || *p == 0) {
	xfree(s);
	return FALSE;
    }
    d = strpbrk(p, "@");
    if (d != NULL) {
	*(d++) = 0;
	for (i = 0; d[i] != 0; i++) {
	    if (!isdigit(d[i])) {
		xfree(s);
		return FALSE;
	    }
	}
    }
    for (i = 0; p[i] != 0; i++) {
	if (!isdigit(p[i])) {
	    xfree(s);
	    return FALSE;
	}
    }
    *bus = atoi(p);
    if (d != NULL && *d != 0)
	*bus += atoi(d) << 8;
    p = strtok(NULL, ":");
    if (p == NULL || *p == 0) {
	xfree(s);
	return FALSE;
    }
    for (i = 0; p[i] != 0; i++) {
	if (!isdigit(p[i])) {
	    xfree(s);
	    return FALSE;
	}
    }
    *device = atoi(p);
    *func = 0;
    p = strtok(NULL, ":");
    if (p == NULL || *p == 0) {
	xfree(s);
	return TRUE;
    }
    for (i = 0; p[i] != 0; i++) {
	if (!isdigit(p[i])) {
	    xfree(s);
	    return FALSE;
	}
    }
    *func = atoi(p);
    xfree(s);
    return TRUE;
}

/*
 * Compare a BUS ID string with a PCI bus id.  Return TRUE if they match.
 */

Bool
xf86ComparePciBusString(const char *busID, int bus, int device, int func)
{
    int ibus, idevice, ifunc;

    if (xf86ParsePciBusString(busID, &ibus, &idevice, &ifunc)) {
	return bus == ibus && device == idevice && func == ifunc;
    } else {
	return FALSE;
    }
}

/*
 * xf86IsPrimaryPci() -- return TRUE if primary device
 * is PCI and bus, dev and func numbers match.
 */

Bool
xf86IsPrimaryPci(pciVideoPtr pPci)
{
    if (primaryBus.type != BUS_PCI) return FALSE;
    return (pPci->bus == primaryBus.id.pci.bus &&
	    pPci->device == primaryBus.id.pci.device &&
	    pPci->func == primaryBus.id.pci.func);
}

/*
 * xf86CheckPciGAType() -- return type of PCI graphics adapter.
 */
int
xf86CheckPciGAType(pciVideoPtr pPci)
{
    int i = 0;
    pciConfigPtr pcp;

    while ((pcp = xf86PciInfo[i]) != NULL) {
	if (pPci->bus == pcp->busnum && pPci->device == pcp->devnum
	    && pPci->func == pcp->funcnum) {
	    if (pcp->pci_base_class == PCI_CLASS_PREHISTORIC &&
		pcp->pci_sub_class == PCI_SUBCLASS_PREHISTORIC_VGA)
		return PCI_CHIP_VGA;
	    if (pcp->pci_base_class == PCI_CLASS_DISPLAY &&
		pcp->pci_sub_class == PCI_SUBCLASS_DISPLAY_VGA) {
		if (pcp->pci_prog_if == PCI_IF_DISPLAY_VGA)
		    return PCI_CHIP_VGA;
		if (pcp->pci_prog_if == PCI_IF_DISPLAY_8514)
		    return PCI_CHIP_8514;
	    }
	    return -1;
	}
	i++;
    }
    return -1;
}

/*
 * xf86GetPciInfoForEntity() -- Get the pciVideoRec of entity.
 */
pciVideoPtr
xf86GetPciInfoForEntity(int entityIndex)
{
    pciVideoPtr *ppPci;
    EntityPtr p;

    if ((entityIndex < 0) || (entityIndex >= xf86NumEntities) ||
	(xf86PciVideoInfo == NULL))
	return NULL;

    p = xf86Entities[entityIndex];
    if (p->busType != BUS_PCI)
	return NULL;

    for (ppPci = xf86PciVideoInfo; *ppPci != NULL; ppPci++) {
	if (p->pciBusId.bus == (*ppPci)->bus &&
	    p->pciBusId.device == (*ppPci)->device &&
	    p->pciBusId.func == (*ppPci)->func)
	    return (*ppPci);
    }
    return NULL;
}

int
xf86GetPciEntity(int bus, int dev, int func)
{
    int i;

    for (i = 0; i < xf86NumEntities; i++) {
	EntityPtr p = xf86Entities[i];
	if (p->busType != BUS_PCI) continue;

	if (p->pciBusId.bus == bus &&
	    p->pciBusId.device == dev &&
	    p->pciBusId.func == func)
	    return i;
    }
    return -1;
}

/*
 * xf86CheckPciMemBase() checks that the memory base value matches one of the
 * PCI base address register values for the given PCI device.
 */
Bool
xf86CheckPciMemBase(pciVideoPtr pPci, memType base)
{
    int i;

    for (i = 0; i < 6; i++)
	if (base == pPci->memBase[i])
	    return TRUE;
    return FALSE;
}

/*
 * Check if the slot requested is free.  If it is already in use, return FALSE.
 */

Bool
xf86CheckPciSlot(int bus, int device, int func)
{
    int i;
    EntityPtr p;

    for (i = 0; i < xf86NumEntities; i++) {
	p = xf86Entities[i];
	if (!p->driver)
	    continue;
	/* Check if this PCI slot is taken */
	if (p->busType == BUS_PCI && p->pciBusId.bus == bus &&
	    p->pciBusId.device == device && p->pciBusId.func == func)
	    return FALSE;
    }

    return TRUE;
}

static int
FindDiscardedPciSlot(int bus, int device, int func)
{
    int i;
    EntityPtr p;

    for (i = 0; i < xf86NumEntities; i++) {
	p = xf86Entities[i];
	if (!p->driver &&
	    p->busType == BUS_PCI && p->pciBusId.bus == bus &&
	    p->pciBusId.device == device && p->pciBusId.func == func) {
	    return i;
	}
    }
    return -1;
}

/*
 * This used to load the scanpci module.  The pcidata module is now used
 * (which the server always loads early).  The main difference between the
 * two modules is size, and the scanpci module should only ever be loaded
 * when the X server is run with the -scanpci flag.
 *
 * To make sure that the required information is present in the pcidata
 * module, add a PCI_VENDOR_* macro for the relevant vendor to xf86PciInfo.h,
 * and add the class override data to ../etc/extrapci.ids.
 */

static void
getPciClassFlags(pciConfigPtr *pcrpp)
{
    pciConfigPtr pcrp;
    int i = 0;

    if (!pcrpp)
	return;
    while ((pcrp = pcrpp[i])) {
	if (!(pcrp->listed_class =
		xf86FindPciClassBySubsys(pcrp->pci_subsys_vendor,
					 pcrp->pci_subsys_card))) {
	    pcrp->listed_class =
		xf86FindPciClassByDevice(pcrp->pci_vendor, pcrp->pci_device);
	}
	i++;
    }
}

/*
 * xf86FindPciVendorDevice() xf86FindPciClass(): These functions
 * are meant to be used by the pci bios emulation. Some bioses
 * need to see if there are _other_ chips of the same type around
 * so by setting pvp_exclude one pci device can be explicitely
 * _excluded if required.
 */
pciVideoPtr
xf86FindPciDeviceVendor(CARD16 vendorID, CARD16 deviceID,
			char n, pciVideoPtr pvp_exclude)
{
    pciVideoPtr pvp, *ppvp;
    n++;

    for (ppvp = xf86PciVideoInfo, pvp =*ppvp; pvp ; pvp = *(++ppvp)) {
	if (pvp == pvp_exclude) continue;
	if ((pvp->vendor == vendorID) && (pvp->chipType == deviceID)) {
	    if (!(--n)) break;
	}
    }
    return pvp;
}

pciVideoPtr
xf86FindPciClass(CARD8 intf, CARD8 subClass, CARD16 class,
		 char n, pciVideoPtr pvp_exclude)
{
    pciVideoPtr pvp, *ppvp;
    n++;

    for (ppvp = xf86PciVideoInfo, pvp =*ppvp; pvp ; pvp = *(++ppvp)) {
	if (pvp == pvp_exclude) continue;
	if ((pvp->interface == intf) && (pvp->subclass == subClass)
	    && (pvp->class == class)) {
	    if (!(--n)) break;
	}
    }
    return pvp;
}

/*
 * This attempts to detect a multi-device card and sets up a list
 * of pci tags of the devices of this card. On some of these
 * cards the BIOS is not visible from all chipsets. We therefore
 * need to use the BIOS from a chipset where it is visible.
 * We do the following heuristics:
 * If we detect only identical pci devices on a bus we assume it's
 * a multi-device card. This assumption isn't true always, however.
 * One might just use identical cards on a bus. We therefore don't
 * detect this situation when we set up the PCI video info. Instead
 * we wait until an attempt to read the BIOS fails.
 */
int
pciTestMultiDeviceCard(int bus, int dev, int func, PCITAG** pTag)
{
  pciConfigPtr *ppcrp = xf86PciInfo;
  pciConfigPtr pcrp = NULL;
  int i,j;
  Bool multicard = FALSE;
  Bool multifunc = FALSE;
  char str[256];
  char *str1;

  str1 = str;
  if (!pTag)
    return 0;

  *pTag = NULL;

  for (i=0; i < 8; i++) {
    j = 0;

    while (ppcrp[j]) {
      if (ppcrp[j]->busnum == bus && ppcrp[j]->funcnum == i) {
	pcrp = ppcrp[j];
	break;
      }
      j++;
    }

    if (!pcrp) return 0;

    /*
     * we check all functions here: since multifunc devices need
     * to implement func 0 we catch all devices on the bus when
     * i = 0
     */
    if (pcrp->pci_header_type & 0x80)
	multifunc = TRUE;

    j = 0;

    while (ppcrp[j]) {
      if (ppcrp[j]->busnum == bus && ppcrp[j]->funcnum == i
	  && ppcrp[j]->devnum != pcrp->devnum) {
	/* don't test subsys ID here. It might be set by POST
	   - however some cards might not have been POSTed */
	if (ppcrp[j]->pci_device_vendor != pcrp->pci_device_vendor
	    || ppcrp[j]->pci_header_type != pcrp->pci_header_type )
	  return 0;
	else
	  multicard = TRUE;
      }
      j++;
    }
    if (!multifunc)
      break;
  }

  if (!multicard)
    return 0;

  j = 0;
  i = 0;
  while (ppcrp[i]) {
    if (ppcrp[i]->busnum == bus && ppcrp[i]->funcnum == func) {
      str1 += sprintf(str1,"[%x:%x:%x]",ppcrp[i]->busnum,
		      ppcrp[i]->devnum,ppcrp[i]->funcnum);
      *pTag = xnfrealloc(*pTag,sizeof(PCITAG) * (j + 1));
      (*pTag)[j++] = pciTag(ppcrp[i]->busnum,
			      ppcrp[i]->devnum,ppcrp[i]->funcnum);
    }
    i++;
  }
  xf86MsgVerb(X_INFO,3,"Multi Device Card detected: %s\n",str);
  return j;
}

static void
pciTagConvertRange2Host(PCITAG tag, resRange *pRange)
{
    if (!(pRange->type & ResBus))
	return;

    switch(pRange->type & ResPhysMask) {
    case ResMem:
	switch(pRange->type & ResExtMask) {
	case ResBlock:
	    pRange->rBegin = pciBusAddrToHostAddr(tag,PCI_MEM, pRange->rBegin);
	    pRange->rEnd = pciBusAddrToHostAddr(tag,PCI_MEM, pRange->rEnd);
	    break;
	case ResSparse:
	    pRange->rBase = pciBusAddrToHostAddr(tag,PCI_MEM_SPARSE_BASE,
						  pRange->rBegin);
	    pRange->rMask = pciBusAddrToHostAddr(tag,PCI_MEM_SPARSE_MASK,
						pRange->rEnd);
	    break;
	}
	break;
    case ResIo:
	switch(pRange->type & ResExtMask) {
	case ResBlock:
	    pRange->rBegin = pciBusAddrToHostAddr(tag,PCI_IO, pRange->rBegin);
	    pRange->rEnd = pciBusAddrToHostAddr(tag,PCI_IO, pRange->rEnd);
	    break;
	case ResSparse:
	    pRange->rBase = pciBusAddrToHostAddr(tag,PCI_IO_SPARSE_BASE
						  , pRange->rBegin);
	    pRange->rMask = pciBusAddrToHostAddr(tag,PCI_IO_SPARSE_MASK
						, pRange->rEnd);
	    break;
	}
	break;
    }

    /* Set domain number */
    pRange->type &= ~(ResDomain | ResBus);
    pRange->type |= xf86GetPciDomain(tag) << 24;
}

static void
pciConvertListToHost(int bus, int dev, int func, resPtr list)
{
    PCITAG tag = pciTag(bus,dev,func);
    while (list) {
	pciTagConvertRange2Host(tag, &list->val);
	list = list->next;
    }
}

static void
updateAccessInfoStatusControlInfo(PCITAG tag, CARD32 ctrl)
{
    int i;

    if (!xf86PciAccInfo)
	return;

    for (i = 0; xf86PciAccInfo[i] != NULL; i++) {
	if (xf86PciAccInfo[i]->arg.tag == tag)
	    xf86PciAccInfo[i]->arg.ctrl = ctrl;
    }
}

void
pciConvertRange2Host(int entityIndex, resRange *pRange)
{
    PCITAG tag;
    pciVideoPtr pvp;

    pvp = xf86GetPciInfoForEntity(entityIndex);
    if (!pvp) return;
    tag = TAG(pvp);
    pciTagConvertRange2Host(tag, pRange);
}


#ifdef INCLUDE_DEPRECATED
void
xf86EnablePciBusMaster(pciVideoPtr pPci, Bool enable)
{
    CARD32 temp;
    PCITAG tag;

    if (!pPci) return;

    tag = pciTag(pPci->bus, pPci->device, pPci->func);
    temp = pciReadLong(tag, PCI_CMD_STAT_REG);
    if (enable) {
	updateAccessInfoStatusControlInfo(tag, temp | PCI_CMD_MASTER_ENABLE);
	pciWriteLong(tag, PCI_CMD_STAT_REG, temp | PCI_CMD_MASTER_ENABLE);
    } else {
	updateAccessInfoStatusControlInfo(tag, temp & ~PCI_CMD_MASTER_ENABLE);
	pciWriteLong(tag, PCI_CMD_STAT_REG, temp & ~PCI_CMD_MASTER_ENABLE);
    }
}
#endif /* INCLUDE_DEPRECATED */
