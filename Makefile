# CXX = g++
CXX = clang++-15
CXXFLAGS = -O3 -march=native

# for crow.cpp
# LIBS = -lpthread

bench.out: bench.cpp
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

.PHONY: run
run: bench.out
	./bench.out

.PHONY: clean
clean:
	rm -f bench.out
