#
#
#

PRJDIR		= ..

include $(PRJDIR)/Makefile.common

USE_ALT_LEX	= 0

PORTS		= $(shell cat ../ports.build)
ALLPORTS	= $(shell cat ../ports.all)
PORT_LIBS	= $(PORTS:%=%/port.a)

LIBS		= -lm 

#CFLAGS          += -Werror

ifdef SDCC_SUB_VERSION
CFLAGS		+= -DSDCC_SUB_VERSION_STR=\"$(SDCC_SUB_VERSION)\"
endif

SLIBOBJS	= SDCCerr.o NewAlloc.o MySystem.o BuildCmd.o dbuf.o

OBJECTS 	= SDCCy.o SDCChasht.o SDCCmain.o \
		  SDCCsymt.o SDCCopt.o SDCCast.o SDCCmem.o SDCCval.o \
		  SDCCicode.o SDCCbitv.o SDCCset.o SDCClabel.o \
		  SDCCBBlock.o SDCCloop.o SDCCcse.o SDCCcflow.o SDCCdflow.o \
		  SDCClrange.o SDCCptropt.o SDCCpeeph.o SDCCglue.o \
		  asm.o SDCCmacro.o SDCCutil.o SDCCdebug.o cdbFile.o

SPECIAL		= SDCCy.h 
ifeq ($(USE_ALT_LEX), 1)
OBJECTS		+= altlex.o
SPECIAL		+= reswords.h
else
OBJECTS		+= SDCClex.o
endif

SOURCES		= $(patsubst %.o,%.c,$(OBJECTS))

TARGET		= $(PRJDIR)/bin/sdcc$(EXEEXT)

# Compiling entire program or any subproject
# ------------------------------------------
all: checkconf $(TARGET)

$(PORT_LIBS): FORCE 
	$(MAKE) -C `dirname $@`
 
FORCE:

$(PRJDIR)/support/gc/libgc.a:
	cd $(PRJDIR)/support/gc && $(MAKE)

# Compiling and installing everything and runing test
# ---------------------------------------------------
install: all installdirs
	$(INSTALL) $(TARGET) `echo $(bindir)/sdcc$(EXEEXT)|sed '$(transform)'`
	$(STRIP) `echo $(bindir)/sdcc$(EXEEXT)|sed '$(transform)'`


# Deleting all the installed files
# --------------------------------
uninstall:
	rm -f $(bindir)/sdcc$(EXEEXT)

# Performing self-test
# --------------------
check:


# Performing installation test
# ----------------------------
installcheck:


# Creating installation directories
# ---------------------------------
installdirs:
	$(INSTALL) -d $(bindir)


# Creating dependencies
# ---------------------
dep: Makefile.dep

Makefile.dep: $(SOURCES) $(SPECIAL) *.h $(PRJDIR)/*.h
	$(CPP) $(CPPFLAGS) $(M_OR_MM) $(SOURCES) >Makefile.dep

include Makefile.dep
include clean.mk


# My rules
# --------
$(TARGET): $(SLIBOBJS) $(OBJECTS) $(PORT_LIBS)
	$(CC) $(LDFLAGS) -o $@ $(SLIBOBJS) $(OBJECTS) $(PORT_LIBS) $(LIBDIRS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(SLIBOBJS):%.o:$(SLIB)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

reswords.h: reswords.gperf Makefile
	gperf -o -k1,2,4 -t -C -N is_reserved_word $< > $@

altlex.o: altlex.c SDCCy.h reswords.h

SDCCy.h: SDCCy.c

SDCCy.c: SDCC.y
	$(YACC) -d -v -o $@ $<

SDCClex.c: SDCC.lex SDCCy.h
	$(LEX) -t $< >$@

.y.c:
	rm -f $*.cc $*.h
	$(YACC) -d $<
	mv y.tab.c $*.cc
	mv y.tab.h $*.h

.l.c:
	rm -f $*.cc
	$(LEX) -t $< >$*.cc


# Remaking configuration
# ----------------------
checkconf:
	@if [ -f $(PRJDIR)/devel ]; then\
	  $(MAKE) -f $(srcdir)/conf.mk srcdir="$(srcdir)" PRJDIR="$(PRJDIR)" \
	  freshconf;\
	fi

# End of main_in.mk/main.mk
