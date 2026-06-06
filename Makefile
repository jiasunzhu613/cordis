CPP = g++
CPP_FLAGS = -Wall -Werror

main: main.cpp utils.cpp utils.hpp
	$(CPP) $(CPP_FLAGS) -o main main.cpp utils.cpp

client: client.cpp utils.cpp utils.hpp
	$(CPP) $(CPP_FLAGS) -o client client.cpp utils.cpp

buffer_test: buffer_test.cpp utils.cpp utils.hpp
	$(CPP) $(CPP_FLAGS) -o buffer_test buffer_test.cpp utils.cpp