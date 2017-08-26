#
#  Makefile for Sun C and FORTRAN versions of Slalom benchmark
#  By Michael B. Carter
#  @(#)Makefile	1.16  11/27/90
#
#  This makefile is set up for a SUN 4.  Change the flags and -DSUN4
#  accordingly to your system.  Currently, there are no other options than
#  -DSUN4, so just remove it if you are running another system.  See dot
#  product at the end of slalom.c for details.
#
CC		= gcc
FC		= f77
CFLAGS	= -O3 $(ARCH) -DSUN4
COFLAGS= 
CLIBES	= -lm
TARGETS	= slalom
CSRC	= slalom.c
SOURCES = $(CSRC) geom Makefile README
DESTDIR = /home/ftp/pub/Slalom/Serial/C

all		:	$(TARGETS)

slalom	:	slalom.c
	@echo "Compiling " slalom.c
	$(CC) $(CFLAGS) $(COFLAGS) $(CPPFLAGS) -o slalom slalom.c $(CLIBES)

ftp		:	$(SOURCES) slalom.sh
	@echo "Copying to ftp directory:" $(SOURCES)
	@for i in $(SOURCES) ; \
	do \
		rm -f $(DESTDIR)/$$i ; \
		cp $$i $(DESTDIR)/$$i ; \
		chmod +w $(DESTDIR)/$$i ; \
	done
	@cp slalom.sh $(DESTDIR)

slalom.sh	:	$(SOURCES)
	shar $(SOURCES) > slalom.sh

delta	:
	@echo "Checking source files into SCCS..."
	@sccs delta $(SOURCES)

edit	:
	@echo "Checking source files out of SCCS..."
	@sccs edit $(SOURCES)
