CPP = g++
CPP_FLAGS = -Wall -Werror

main: main.cpp utils.cpp utils.hpp hashtable.cpp hashtable.hpp
	$(CPP) $(CPP_FLAGS) -o main.out main.cpp utils.cpp hashtable.cpp

client: client.cpp utils.cpp utils.hpp hashtable.cpp hashtable.hpp
	$(CPP) $(CPP_FLAGS) -o client.out client.cpp utils.cpp hashtable.cpp

buffer_test: buffer_test.cpp utils.cpp utils.hpp hashtable.cpp hashtable.hpp
	$(CPP) $(CPP_FLAGS) -o buffer_test.out buffer_test.cpp utils.cpp hashtable.cpp