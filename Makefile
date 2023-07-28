# CXX = g++
CXX = clang++-15
CXXFLAGS = -O3 -march=native

# for crow.cpp
# LIBS = -lpthread

mcts.out: mcts.cpp
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

.PHONY: run
run: mcts.out
	./mcts.out

.PHONY: clean
clean:
	rm -f mcts.out
