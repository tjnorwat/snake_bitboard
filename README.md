# List of improvements

## Changing from vectors/dynamic arrays to static array for body arr 
Originally, I was using vectors to store the indexes in the specific order of where the head moves. This order is important due to the fact that we have to remove the tail from the players whenever they make a move (as long as they dont consume food). I was testing third party vector types as well to get that extra bit of performance but only marginal gains were to be found.

After taking a break from the project, I realized that we could store the indexes in an array that is atleast the size of the board with 2 pointers to where the head and tail indexes are supposed to be. This allows for static allocation of memory and all we are doing is incrementing/decrementing those pointers and writing those to the array, which is a lot faster than dynamically allocating/deallocating memory every turn. 

This ended up being a 4x performance improvement.
* went from about 4500 ms to 950-1000 ms (All of these times are measured at depth 15)
## Getting rid of lsb function calls 
I was storing the positions of the board using the boost multiprecision library. This library offers integers larger than 64 bits and since the game is a size of 11x11, that mean I could use the 128bit integer. This allowed for easy manipulation of bits and bitwise operation. I was regularly getting the index of the head to compute possible moves and once I got the possible moves, used the lsb function to get the indexes of those moves. 

```cpp
uint16_t head_idx = boost::multiprecision::lsb(head_board);
uint128_t move_board = uint128_t(0);

move_board |= uint128_t(1) << (head_idx + 1);
move_board |= uint128_t(1) << (head_idx - 1);
move_board |= uint128_t(1) << (head_idx + 11);
move_board |= uint128_t(1) << (head_idx - 11);
```
```cpp
uint128_t my_move_board = possible_moves(this->me.snake_head_board);
uint128_t opponent_move_board = possible_moves(this->opponent.snake_head_board);

while(my_move_board) {
    uint16_t my_move = boost::multiprecision::lsb(my_move_board);
    ...
}
```
Trying out different things, I stumbled upon caching the index of the head which did increase performance. After realizing this, getting rid of all lsb calls would prove to be about a 2x speedup.


* went from 950 ms to 525ms getting rid of move lsb 

## Caching move values
How did I get rid of using the lsb function? By precomputing the possible moves for all directions and indexes on the board. We can then get our possible moves by just getting our direction and the index of our head
```cpp
const vector<uint16_t>& my_move_board = precompute_moves[me.direction][me.body_arr[me.head_idx & ARR_SIZE]];
for (const uint16_t my_move : my_move_board) {
    ...
}
```
With this, I saw about a 12% improvement since we no longer have to calculate possible moves every step.
* went from 525 ms to about 460ms

## Passing by reference in minimax
Seems intuitive when you think about it. Passing by value means that we are creating a copy for each object we pass in the function. If we pass by reference we don't need to create a copy of anything, we just pass the memory address. This doesn't have any affect on the variables since I was already creating a temp variable to be manipulated instead of having an undo function. 

Allowed for another 12% speedup 
* 460 ms to about 400 ms

## Clang faster than g++
Clang is simply more efficient than g++. Switching over to Clang is free performance essentially and saw about a 22% improvement
* 410 ms to 320ms

## Pairs of 64bit ints 
Instead of using boost for the 128bit integers, switching to pairs of 64bit integers seemed to offer a big gain of performance. This is most likely due to being natively supported as well as boost has some overhead to support the larger int types. 

**Old step function**
```cpp
void step_by_index(uint16_t idx) {
    uint128_t new_head = uint128_t(1) << idx;
    this->body_list.push_front(idx);
    this->snake_body_board |= this->snake_head_board;
    this->old_head_board = this->snake_head_board;
    this->snake_head_board = new_head;
    int dir = idx - boost::multiprecision::lsb(this->old_head_board);

    switch (dir) {
    case 1:
        this->direction = LEFT;
        break;
    case -1:
        this->direction = RIGHT;
        break;
    case 11:
        this->direction = UP;
        break;
    default:
        this->direction = DOWN;
    }
}
```
**New step function**
```cpp
void step_by_index(uint16_t idx) {
    // getting new direction before we increment head idx 
    this->direction = direction_lookup[idx - this->body_arr[(this->head_idx) & ARR_SIZE] + BOARD_SIZE];

    this->head_idx++;
    this->body_arr[this->head_idx & ARR_SIZE] = idx;

    // Add current head position to body board
    this->snake_body_board_firsthalf |= this->snake_head_board_firsthalf;
    this->snake_body_board_secondhalf |= this->snake_head_board_secondhalf;

    // Save current head position
    this->old_head_board_firsthalf = this->snake_head_board_firsthalf;
    this->old_head_board_secondhalf = this->snake_head_board_secondhalf;

    // Set new head position
    // always will have one of the two set to 0
    if (idx < 64) {
        this->snake_head_board_firsthalf = 1ULL << idx;
        this->snake_head_board_secondhalf = 0ULL;
    }
    else { 
        this->snake_head_board_secondhalf = 1ULL << (idx & 63);
        this->snake_head_board_firsthalf = 0ULL;
    }
}
```
Here is a comparison of old and new step function, which is a little messier, but worth the performance improvement and we don't have to depend on boost anymore. 
* 320 ms to 265 ms

## Conclusion
The final timings come into about 27.2 nanoseconds per turn at depth 15, with almost 10 million positions being checked. The minimax evaluation is still a basic implementation, but my goal was to get the main logic to be efficient and then work on heuristics. I would like to be able to switch to MCTS and deep learning for the evaluation and exploration of the game tree.  