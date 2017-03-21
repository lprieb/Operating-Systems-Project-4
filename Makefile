CC_FLAGS = -g -gdwarf-2 -std=c++0x

project4: project4.cpp
	g++ $(CC_FLAGS) $^ -o $@
