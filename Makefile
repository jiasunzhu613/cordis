CPP = g++
CPP_FLAGS = -Wall -Werror

main: main.cpp
	$(CPP) $(CPP_FLAGS) -o main main.cpp utils.cpp

client: client.cpp
	$(CPP) $(CPP_FLAGS) -o client client.cpp utils.cpp