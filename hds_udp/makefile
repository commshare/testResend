CC=g++
CFLAGS = -Wall
export DEBUG = y
export USE_BASE_SOCKET = y

ifeq ($(DEBUG), y)
CFLAGS += -g
else
CFLAGS += -O2
endif

SUBDIRS := common kernel include connection method_processor buffer TCP

LIBS := -lpthread

LDFLAGS = $(LIBS)

RM = -rm -rf

MAKE = make

target = lhdsudp.a

OBJS = $(wildcard obj/*.o)
all: 
	for dir in $(SUBDIRS); \
	do $(MAKE) -C $$dir all || exit 1; \
	done
	ar cqs obj/libHDS_UDP.a $(OBJS)

clean:
	@for dir in $(SUBDIRS); do make -C $$dir clean|| exit 1; done
	rm obj/*.o
	rm obj/*.a

.PHONY: all clean 
