CPP      := g++
CPPFLAGS := -pthread
SRC      := bitmap.cpp utils.cpp
HDR      := utils.hpp

all: gather barrier

gather: $(SRC) $(HDR)
	$(CPP) $(CPPFLAGS) $(SRC) -o gather

barrier: $(SRC) $(HDR)
	$(CPP) $(CPPFLAGS) -DUSE_BARRIER $(SRC) -o barrier

clean:
	rm -f gather barrier 

