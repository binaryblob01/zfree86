/*

Copyright 1993, 1998  The Open Group

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

*/
/* $XFree86: xc/lib/SM/sm_misc.c,v 1.3 2005/03/23 03:11:25 dawes Exp $ */

/*
 * Author: Ralph Mor, X Consortium
 */

#include <X11/SM/SMlib.h>
#include "SMlibint.h"
#include <stdio.h>

/*
 * Free property
 */

void
SmFreeProperty(SmProp *prop)
{
    if (prop)
    {
	int i;

	if (prop->name)
	    free (prop->name);
	if (prop->type)
	    free (prop->type);
	if (prop->vals)
	{
	    for (i = 0; i < prop->num_vals; i++)
		if (prop->vals[i].value)
		    free ((char *) prop->vals[i].value);
	    free ((char *) prop->vals);
	}

	free ((char *) prop);
    }
}


/*
 * Free reason messages
 */

void
SmFreeReasons(int count, char **reasonMsgs)
{
    if (reasonMsgs)
    {
	int i;

	for (i = 0; i < count; i++)
	    free (reasonMsgs[i]);

	free ((char *) reasonMsgs);
    }
}


/*
 * Smc informational functions
 */

int
SmcProtocolVersion(SmcConn smcConn)
{
    return (smcConn->proto_major_version);
}


int
SmcProtocolRevision(SmcConn smcConn)
{
    return (smcConn->proto_minor_version);
}


char *
SmcVendor(SmcConn smcConn)
{
    char *string = (char *) malloc (strlen (smcConn->vendor) + 1);

    strcpy (string, smcConn->vendor);

    return (string);
}


char *
SmcRelease(SmcConn smcConn)
{
    char *string = (char *) malloc (strlen (smcConn->release) + 1);

    strcpy (string, smcConn->release);

    return (string);
}


char *
SmcClientID(SmcConn smcConn)
{
    char *clientId = (char *) malloc (strlen (smcConn->client_id) + 1);

    strcpy (clientId, smcConn->client_id);

    return (clientId);
}


IceConn
SmcGetIceConnection(SmcConn smcConn)
{
    return (smcConn->iceConn);
}


/*
 * Sms informational functions
 */

int
SmsProtocolVersion(SmsConn smsConn)
{
    return (smsConn->proto_major_version);
}


int
SmsProtocolRevision(SmsConn smsConn)
{
    return (smsConn->proto_minor_version);
}


char *
SmsClientID(SmsConn smsConn)
{
    char *clientId = (char *) malloc (strlen (smsConn->client_id) + 1);

    strcpy (clientId, smsConn->client_id);

    return (clientId);
}


IceConn
SmsGetIceConnection(SmsConn smsConn)
{
    return (smsConn->iceConn);
}
