#!/bin/sh

#
# Copyright 2003-2005 by David H. Dawes.
# Copyright 2003-2005 by X-Oz Technologies.
# All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions, and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above
#     copyright notice, this list of conditions and the following
#     disclaimer in the documentation and/or other materials provided
#     with the distribution.
# 
#  3. The end-user documentation included with the redistribution,
#     if any, must include the following acknowledgment: "This product
#     includes software developed by X-Oz Technologies
#     (http://www.x-oz.com/)."  Alternately, this acknowledgment may
#     appear in the software itself, if and wherever such third-party
#     acknowledgments normally appear.
#
#  4. Except as contained in this notice, the name of X-Oz
#     Technologies shall not be used in advertising or otherwise to
#     promote the sale, use or other dealings in this Software without
#     prior written authorization from X-Oz Technologies.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL X-OZ TECHNOLOGIES OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Author: David Dawes <dawes@XFree86.Org>.
#

# $XFree86: xc/programs/Xserver/hw/xfree86/getconfig/getconfig.sh,v 1.6 2005/12/31 17:36:34 dawes Exp $

# A simple wrapper to execute the real getconfig program.  So long as perl
# is in $PATH, we don't need to know where it is this way.

if echo $0 | grep / >/dev/null 2>&1; then
	DIR=`dirname $0`/
fi

# Avoid locale warnings from perl.
LC_ALL=C
export LC_ALL

if (perl -e 'print int $];' || exit 1) >/dev/null 2>&1; then
	: OK, we have perl
else
	echo "$0: perl not found." >&2 
	exit 1
fi

perl_vers=`perl -e 'print int $];'`
if [ $perl_vers -lt 5 ]; then
	echo "$0: perl version is too old (need 5 or later)." >&2 
	exit 1
fi

if [ "X$1" = "X-D" ]; then
	echo "executing: perl ${DIR}getconfig.pl $@" >&2
fi

exec perl ${DIR}getconfig.pl "$@"
