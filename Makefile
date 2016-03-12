#!gmake 
#====================================================================
#
# Makefile for the new version of the dgt driver engine based upon
# dgtnix interface.
#
# $Id: Makefile,v 1.10 2008/11/16 12:23:15 arwagner Exp $
#
# Last change: <Sun, 2008/11/16 11:47:05 arwagner ingata>
#
#====================================================================
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

# Real board:
PORT     = /dev/ttyUSB0
# Virtual board:
# PORT     = /tmp/dgtnixBoard
PARAMS   = la
INCLUDES = /opt/chess/include
LIBS     = /opt/chess/lib

PLATFORM := $(shell uname -m | sed -e 's/ //g')

CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -ansi
CXXFLAGS = -O2 -Wall -ansi
LINK     = gcc
LFLAGS   = -shared -Wall

####### Files

HEADERS = dgtnix.h
SOURCES = dgtdrv2.cpp
OBJECTS = dgtdrv2.o
TARGET  = dgtdrv2
savdir = ${PWD}

first: all
####### Implicit rules

.SUFFIXES: .c .o .cpp .cc .cxx .C

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.C.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

####### Build Rules

all: Makefile $(TARGET).$(PLATFORM)
	cp $(TARGET).$(PLATFORM) /opt/dgtDriver/dgtdrv/out
	groff -man -T html dgtdrv.3 > dgtdrv-3.html
	groff -man -T html dgtdrv.6 > dgtdrv-6.html

########## Create a package .tar.gz
dist: distclean

distclean: clean
	rm -f $(TARGET).$(PLATFORM)
	rm -f dgtdrv-3.html
	rm -f dgtdrv-6.html

clean:
	rm -f $(TARGET).$(PLATFORM)
	rm -f $(OBJECTS)
	rm -f *~ core *.core \#*\#

####### TEST APPLICATION

$(TARGET).$(PLATFORM): $(SOURCES) $(HEADER)
	-$(CXX)  -lpthread -ldgtnix -I$(INCLUDES) -L$(LIBS) $(SOURCES) -o $(TARGET).$(PLATFORM)

run: $(TARGET).$(PLATFORM)
	./$(TARGET).$(PLATFORM) $(PORT) $(PARAMS)

xboard: $(TARGET).$(PLATFORM)
	xboard-dgt -size Small -debug -fcp "$(TARGET).$(PLATFORM) $(PORT) $(PARAMS)" -scp gnuchessx
