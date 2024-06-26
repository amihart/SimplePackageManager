# Generated automatically from Makefile.in by configure.
#
# Makefile
#   Copyright (C) 1999
#   Free Software Foundation
#
# This file is part of the libiberty library.
# Libiberty is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Libiberty is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with libiberty; see the file COPYING.LIB.  If not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

# This file was written by Tom Tromey <tromey@cygnus.com>.

#
# Makefile for libiberty/testsuite directory
#

srcdir = .

SHELL = /bin/bash

CC = ucfront-gcc arm-linux-elf-gcc 
CFLAGS = -O2 -g -fomit-frame-pointer -fno-common -fno-builtin -Wall  -mswp-byte-writes -DCONFIG_NDS_ROM8BIT -mcpu=arm946e-s -mfpu=fpe3 -DEMBED -D__PIC__ -fpic -msingle-pic-base -Dlinux -D__linux__ -Dunix -D__uClinux__
LIBCFLAGS = $(CFLAGS)

# Multilib support variables.
MULTISRCTOP =

INCDIR=$(srcdir)/../$(MULTISRCTOP)../include

all:

check: check-cplus-dem

# Run some tests of the demangler.
check-cplus-dem: test-filter $(srcdir)/demangle-expected
	$(SHELL) $(srcdir)/regress-demangle $(srcdir)/demangle-expected

# Note that we just hard-code prepends_underscore to 0.  This doesn't
# matter since any particular test can override the default if need
# be.
TEST_COMPILE = $(CC) -DHAVE_CONFIG_H $(LIBCFLAGS) -I.. -I$(INCDIR) $(HDEFINES)
test-filter: $(srcdir)/../cplus-dem.c
	echo 'int prepends_underscore = 0;' > test-us.c
	$(TEST_COMPILE) -o test-filter -DMAIN -DVERSION='"none"' -DHAVE_CONFIG_H \
		$(srcdir)/../cplus-dem.c test-us.c -L.. -liberty


# Standard (either GNU or Cygnus) rules we don't use.
info install-info clean-info dvi install etags tags installcheck:

# The standard clean rules.
mostlyclean:
	rm -f test-us.c test-filter
clean: mostlyclean
distclean: clean
	rm -f Makefile
maintainer-clean realclean: distclean


Makefile: $(srcdir)/Makefile.in ../config.status
	CONFIG_FILES=testsuite/Makefile CONFIG_HEADERS= \
	  cd .. && $(SHELL) ./config.status
