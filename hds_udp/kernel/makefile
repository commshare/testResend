###############Author:jsunp##############
##############date :2012########
CC = g++

CXXFLAGS = -Wall
ifeq ($(DEBUG), y)
CXXFLAGS += -g
else
CXXFLAGS += -O2
endif

ifeq ($(USE_BASE_SOCKET), y)
SUBDIRS = sock
else
SUBDIRS = epoll
endif

OUTDIR=../obj/

VPATH = -I/usr/local/boost

LIBS = -lpthread

MAKE = make

COMMON_OBJECTS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(COMMON_OBJECTS)
	for dir in $(SUBDIRS); \
	do $(MAKE) -C $$dir all || exit 1; \
	done	
$(COMMON_OBJECTS) : %.o :%.cpp %.h
	$(CC) -c $(CXXFLAGS) -o $(OUTDIR)$@ $(VPATH) $<

.PHONY : clean
clean : 
	rm -f *.o
	@for dir in $(SUBDIRS); do make -C $$dir clean|| exit 1; done

export COMMON_OBJECTS
