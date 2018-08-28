###############Author:jsunp##############
##############date :2012########
CC = g++

CXXFLAGS = -Wall
ifeq ($(DEBUG), y)
CXXFLAGS += -g
else
CXXFLAGS += -O2
endif

OUTDIR=../obj/

VPATH = -I/usr/local/boost

LIBS = -lpthread

MAKE = make

COMMON_OBJECTS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(COMMON_OBJECTS)
$(COMMON_OBJECTS) : %.o :%.cpp %.h
	$(CC) -c $(CXXFLAGS) -o $(OUTDIR)$@ $(VPATH) $<

.PHONY : clean
clean : 
	rm -f *.o
	@for dir in $(SUBDIRS); do make -C $$dir clean|| exit 1; done

export COMMON_OBJECTS
