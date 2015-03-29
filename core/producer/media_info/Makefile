SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

CC = g++
CFLAGS = -Wall -O2 -std=gnu++11 -fpermissive -DBOOST_LOG_DYN_LINK
INCLUDES = -I/home/sysnova/workspace/casparLinux/dependencies/boost/ -I/home/sysnova/workspace/casparLinux/ -I/home/sysnova/workspace/casparLinux/dependencies/tbb/include/ -I../../../dependencies/glew/include/

all: $(OBJ)

$(OBJ):%.o:%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	@rm -rf $(OBJ)
	@rm -rf *~
