###############Author:jsunp##############
##############date :2012########
CC = g++

CXXFLAGS = -Wall
ifeq ($(DEBUG), y)
CXXFLAGS += -g
else
CXXFLAGS += -O2
endif

OUTDIR =../../obj/

VPATH = -I/usr/local/boost

LIBS = -lpthread

MAKE = make

COMMON_OBJECTS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(COMMON_OBJECTS)
#libCommon.a:%.o	
$(COMMON_OBJECTS) : %.o :%.cpp %.h
	$(CC) -c $(CXXFLAGS) -o $(OUTDIR)$@ $(VPATH) $<

.PHONY : clean
clean : 
	rm -f *.o

export COMMON_OBJECTS
