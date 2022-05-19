CPP = g++
COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare -std=c++11

LINKLIBS = -lpthread

OBJECTS = obj/mp1_node.o obj/util.o obj/connectHandler.o

.PHONY: all clean

all: obj mp1_node

clean:
	$(RM) obj/*.o mp1_node

mp1_node: $(OBJECTS)
	$(CPP) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

obj/%.o: %.cc
	$(CPP) $(COMPILERFLAGS) -c -o $@ $<

obj:
	mkdir -p obj