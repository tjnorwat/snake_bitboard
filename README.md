# changing from vectors/ dynamic arrays to static array for body arr 


# getting rid of lsb function calls 
* went from 950 ms to 525ms getting rid of move lsb 



# caching move values

* tried caching with 2d arr; arr had uint128_t possible moves

* caching actual indexes to get rid of lsb calls 
    * tried using 3d array but possible moves wasnt always size 3 
        * once removed if move_idx was > 120 break ; saw 12% uplift 