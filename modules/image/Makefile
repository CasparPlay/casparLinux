include Makefile.inc

SUBDIRS = consumer producer util
	
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

CC = g++
CFLAGS = -Wall -O2 -std=gnu++11 -fpermissive -DBOOST_LOG_DYN_LINK
INCLUDES = -I../../dependencies/boost/ -I../../ -I../dependencies/tbb/include/ -I../dependencies/glew/include/ -I../../dependencies/ffmpeg/include/ -I../../dependencies/FreeImage/Dist/

all clean:
	list='$(SUBDIRS)'; \
	for subdir in $$list; do \
		$(MAKE) -C $$subdir $@ || exit 1; \
	done

