CPP      := g++
CPPFLAGS := -pthread
SRC      := bitmap.cpp

all: gather barrier

gather: $(SRC)
	$(CPP) $(CPPFLAGS) $(SRC) -o $@

barrier: $(SRC) 
	$(CPP) $(CPPFLAGS) -DUSE_BARRIER $(SRC) -o $@

clean:
	rm -f gather barrier 

