SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

CC = g++
CFLAGS = -Wall -O2 -std=gnu++11 -fpermissive -DBOOST_LOG_DYN_LINK
INCLUDES = -I../dependencies/boost/ -I../ -I../dependencies/tbb/include/ -I../dependencies/glew/include/

all: $(OBJ)

$(OBJ): server.o main.o

server.o: main.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

main.o:	server.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -rf $(OBJ)
	rm -rf *~
