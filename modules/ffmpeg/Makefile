include Makefile.inc

SUBDIRS = consumer \
	producer producer/audio producer/filter producer/input producer/input \
	producer/muxer producer/util producer/video \
	producer/util
	
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

CC = g++
CFLAGS = -Wall -O2 -std=gnu++11 -fpermissive -DBOOST_LOG_DYN_LINK
INCLUDES = -I../../dependencies/boost/ -I../../ -I../dependencies/tbb/include/ -I../dependencies/glew/include/ -I../../dependencies/ffmpeg/include/

all clean:
	list='$(SUBDIRS)'; \
	for subdir in $$list; do \
		$(MAKE) -C $$subdir $@ || exit 1; \
	done

