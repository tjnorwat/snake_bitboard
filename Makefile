# CXX = g++
CXX = clang++-15
CXXFLAGS = -O3 -march=native
LIBS = -lpthread

server.out: server.cpp
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

.PHONY: run
run: server.out
	./server.out

.PHONY: clean
clean:
	rm -f server.out
