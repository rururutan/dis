# Makefile for AVL-tree library.
# Copyright (C) 1991 K.Abe

#CFLAGS        = -Wall -O -fomit-frame-pointer -fstrength-reduce -finline-functions\
#		-fcombine-regs -fforce-mem -fforce-addr -fcse-follow-jumps\
#		-DPROFILE -DAVL_NOMACRO -DAVL_LIBRARY

CFLAGS        = -Wall -O -fomit-frame-pointer -finline-functions\
		-fcombine-regs -fforce-mem -fforce-addr\
		-DPROFILE -DAVL_LIBRARY

#CC            = gcc2.x
CC            = gcc

EXTHDRS	      =

HDRS	      = avl.h

ARCHIVE_MEMBER = avl.c avl.h avl.tex Makefile TAGS

LDFLAGS	      =

LIBS	      =

LINKER	      = gcc

MAKEFILE      = Makefile

OBJS	      = avl.o \
#		avltest.o

#PROGRAM	= avl.o

SRCS	      = avl.c \
#		avltest.c

.PHONY:		all clean depend index program

all:		avl.o avllib.Lzh

avllib.Lzh:	$(ARCHIVE_MEMBER)
		lha a $@ $?

#$(PROGRAM):     $(OBJS) $(LIBS)
#		$(LINKER) $(LDFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)

clean:;		rm -f $(OBJS)

depend:;	mkmf -f $(MAKEFILE) PROGRAM=$(PROGRAM)

index:;		ctags -wx $(HDRS) $(SRCS)

program:        $(PROGRAM)

TAGS:           $(HDRS) $(SRCS)
		etags -et $(HDRS) $(SRCS)

###
avl.o: avl.h
#avltest.o: avl.h
