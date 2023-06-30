# changing from vectors/ dynamic arrays to static array for body arr 


# getting rid of lsb function calls 
* went from 950 ms to 525ms getting rid of move lsb 



# caching move values

* tried caching with 2d arr; arr had uint128_t possible moves

* caching actual indexes to get rid of lsb calls 
    * tried using 3d array but possible moves wasnt always size 3 
        * once removed if move_idx was > 120 break ; saw 12% uplift 

# passing by reference in minimax
* allowed for an 12% speedup 

# clang faster than g++
* 22% improvement 

# pairs of 64bit ints 
* 20% improvement
* might be because of native support and library overhead of 128bit 