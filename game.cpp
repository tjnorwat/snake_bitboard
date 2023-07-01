#include <deque>
#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include "player.cpp"

using boost::multiprecision::uint128_t;
using namespace std;


// constexpr uint128_t OUT_OF_BOUNDS = 0xFE000000000000000000000000000000;
// constexpr uint128_t FIRST_COLUMN = 0x1002004008010020040080100200400;
// constexpr uint128_t LAST_COLUMN = 0x4008010020040080100200400801;
// constexpr uint128_t FIRST_ROW = 0x1FFC000000000000000000000000000;


struct Game {
    bool done;
    uint16_t size;
    Player me;
    Player opponent;
    int16_t total_turns;
    uint128_t food_board;
    uint128_t first_column;
    uint128_t last_column;
    uint128_t first_row;
    uint128_t last_row;
    uint128_t out_of_bounds;
    std::vector<uint32_t> food_choices;

    Game();
    Game(uint16_t size);
    void step(uint16_t action);
    void get_food();
    void reset(); // could maybe call in /end? 
    void print_board() const;
    void print_individual_board(uint128_t board) const;
    void set_starting_position(uint16_t snake1_idx, uint16_t snake2_idx, vector<uint16_t> food_indexes); // /start endpoint 
    void update_positions(vector<uint16_t> food_indexes, uint16_t my_health, uint16_t opponent_health); // will take care of /move endpoint 
    int minimax(Player me, Player opponent, uint128_t food_board, uint16_t depth, int alpha, int beta, bool maximizing_player);
    uint16_t find_best_move();
    void update_opponent_move(int move);
    int evaluate(Player me, Player opponent, uint128_t food_board);
    uint128_t possible_moves(uint128_t head_board);
};

Game::Game() {}

Game::Game(uint16_t size) : size(size), done(false) {

    first_column = uint128_t(0);
    last_column = uint128_t(0);
    first_row = uint128_t(0);
    last_row = uint128_t(0);

    for (uint16_t i = 0; i < size; ++i) {
        first_column |= uint128_t(1) << (size * i + size - 1);
        last_column |= uint128_t(1) << (size * i);
        first_row |= uint128_t(1) << (size * size - i - 1);
        last_row |= uint128_t(1) << i;
    }

    out_of_bounds = uint128_t(0);
    for (int i = 121; i < 128; ++i) {
        out_of_bounds |= uint128_t(1) << i;
    }

    // cout << "out of bounds " << out_of_bounds << endl;
    // cout << "first column " << first_column << endl;
    // cout << "last column " << last_column << endl;
    // cout << "first row " << first_row << endl;

    food_board = uint128_t(0);

}

// food indexes should always be 3 
void Game::set_starting_position(uint16_t mysnake_idx, uint16_t opponent_idx, vector<uint16_t> food_indexes) {
    // need to get starting food locations 
    // starting snake positions 

    this->me = Player(mysnake_idx);
    this->opponent = Player(opponent_idx);

    for (uint16_t idx : food_indexes)
        this->food_board |= uint128_t(1) << idx;
}

// updates the move from oponent after first turn 
void Game::update_opponent_move(int new_head_idx) {
    int old_head_idx = boost::multiprecision::lsb(this->opponent.snake_head_board);
    
    int diff = new_head_idx - old_head_idx;
    switch (diff)
    {
    case 1: // left 
        this->opponent.step(0);
        break;
    case -1: // right
        this->opponent.step(1);
        break;
    case 11: // up
        this->opponent.step(2);
        break;
    case -11: // down
        this->opponent.step(3);
        break;
    default:
        break;
    }
}


// for /move endpoint , updates food and makes sure everything is ready for finding the best move 
void Game::update_positions(vector<uint16_t> food_indexes, uint16_t my_health, uint16_t opponent_health) {
    // just reseting the food for now 
    this->food_board = uint128_t(0);
    for (uint16_t idx : food_indexes)
        this->food_board |= uint128_t(1) << idx;

    this->me.health = my_health;
    this->opponent.health = opponent_health;

}


int Game::evaluate(Player me, Player opponent, uint128_t food_board) {

    // check if player hit wall first 
    // check wall first because if we also run into opponents body when they hit wall, we dont die 
    if (me.snake_head_board == 0) // down
        return -1000;
    else if (me.old_head_board & first_column && me.snake_head_board & last_column) // left
        return -1000;
    else if (me.old_head_board & last_column && me.snake_head_board & first_column) // right
        return -1000;
    else if (me.old_head_board & first_row && me.direction == 2) // up
        return -1000;

    // opponent runs into wall
    if (opponent.snake_head_board == 0)
        return 1000; 
    else if (opponent.old_head_board & first_column && opponent.snake_head_board & last_column)
        return 1000;
    else if (opponent.old_head_board & last_column && opponent.snake_head_board & first_column)
        return 1000;
    else if (opponent.old_head_board & first_row && opponent.direction == 2)
        return 1000;


    // if either runs into others head, check which would win 
    if (me.snake_head_board & opponent.snake_head_board){
        if (me.length > opponent.length) {
            return 1000;
        }
        else if (me.length < opponent.length) {
            return -1000;
        }
        // still return that we 'lost' even though it was a draw
        return -1000;
    }

    uint128_t all_boards = me.snake_body_board | opponent.snake_body_board;
    // if either snake runs into itself / opponent
    if (me.snake_head_board & all_boards) {
        return -1000;
    }
    else if (opponent.snake_head_board & all_boards) {
        return 1000;
    }


    // if either player runs out of health
    if (me.health < 0) {
        return -1000;
    }
    else if (opponent.health < 0) {
        return 1000;
    }

    // checking if we pick up apple 
    // need a better way of doing this 
    if (me.snake_head_board & food_board){
        return 10;
    }
    else if (opponent.snake_head_board & food_board) {
        return -10;
    }

    // todo : make hueristic about scoring where we are on the board

    return 0;
}


int Game::minimax(Player me, Player opponent, uint128_t food_board, uint16_t depth, int alpha, int beta, bool maximizing_player) {
    
    int score = evaluate(me, opponent, food_board);

    // terminal state ; if depth is 0 or the game has ended; can check if game has ended by the score (player dies) 
    if (depth == 0) {
        return score;
    }
    // these are terminal states 
    else if (score == 1000) {
        return score + depth;
    }
    else if (score == -1000) {
        return score - depth;
    }

    // adjusting the food board 
    if (score == 10) {
        food_board ^= me.snake_head_board;
    }
    else if (score == -10) {
        food_board ^= opponent.snake_head_board;
    }
    
    if (maximizing_player) {
        int max_eval = INT32_MIN;
        uint128_t my_move_board = possible_moves(this->me.snake_head_board);
        uint128_t opponent_move_board = possible_moves(this->opponent.snake_head_board);

        while(my_move_board) {
            uint16_t my_move = boost::multiprecision::lsb(my_move_board);
            Player temp_me = me;
            me.step_by_index(my_move);
            while(opponent_move_board) {

                uint16_t opponent_move = boost::multiprecision::lsb(opponent_move_board);
                opponent_move_board ^= opponent_move;
                
                // can we keep an old reference to 'undo' the move?
                Player temp_opponent = opponent;
                opponent.step_by_index(opponent_move);

                int move_val = minimax(me, opponent, food_board, depth - 1, INT32_MIN, INT32_MAX, false);

                max_eval = max(max_eval, move_val);
                alpha = max(alpha, move_val);
                
                if (beta <= alpha) {
                    break;
                }

                // undo opponent;
                opponent = temp_opponent;
            }
            my_move_board ^= my_move;
            // undo my move
            me = temp_me;
        }
        return max_eval;
    }
    else {

        int min_eval = INT32_MAX;
        uint128_t my_move_board = possible_moves(this->me.snake_head_board);
        uint128_t opponent_move_board = possible_moves(this->opponent.snake_head_board);

        while(my_move_board) {
            uint16_t my_move = boost::multiprecision::lsb(my_move_board);
            Player temp_me = me;
            me.step_by_index(my_move);
            while(opponent_move_board) {

                uint16_t opponent_move = boost::multiprecision::lsb(opponent_move_board);
                opponent_move_board ^= opponent_move;
                
                // can we keep an old reference to 'undo' the move?
                Player temp_opponent = opponent;
                opponent.step_by_index(opponent_move);

                int move_val = minimax(me, opponent, food_board, depth - 1, INT32_MIN, INT32_MAX, true);

                min_eval = min(min_eval, move_val);
                beta = max(beta, move_val);
                
                if (beta <= alpha) {
                    break;
                }

                // undo opponent;
                opponent = temp_opponent;
            }
            my_move_board ^= my_move;
            // undo my move
            me = temp_me;
        }
        return min_eval;
    }
}

// create enum for directions 
// will be iterative deepening 
// maybe later can calculate the time limit based on the first response 
uint16_t Game::find_best_move() {

    int best_val = INT32_MAX;
    uint16_t best_move; 

    // uint128_t move_board = uint128_t(0);
    // uint16_t my_head_idx = boost::multiprecision::lsb(this->me.snake_head_board);
    // move_board |= uint128_t(1) << (my_head_idx + 1);
    // move_board |= uint128_t(1) << (my_head_idx - 1);
    // move_board |= uint128_t(1) << (my_head_idx + 11);
    // move_board |= uint128_t(1) << (my_head_idx - 11);
    // need to take into account if on the edge of the board and if body parts are in the way 

    uint128_t my_move_board = possible_moves(this->me.snake_head_board);
    uint128_t opponent_move_board = possible_moves(this->opponent.snake_head_board);

    Player copy_me = this->me;
    Player copy_opponent = this->opponent;
    uint128_t copy_food_board = this->food_board;

    int depth = 0;
    auto start_time = chrono::high_resolution_clock::now();

    

    // will have to figure out how to get possible moves for players 
    while (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count() < 300) {
        cout << "TIME " << (chrono::high_resolution_clock::now() - start_time).count() << endl;
        while(my_move_board) {
            uint16_t my_move = boost::multiprecision::lsb(my_move_board);
            Player temp_me = copy_me;
            copy_me.step_by_index(my_move);
            while(opponent_move_board) {

                uint16_t opponent_move = boost::multiprecision::lsb(opponent_move_board);
                opponent_move_board ^= opponent_move;
                
                // can we keep an old reference to 'undo' the move?
                Player temp_opponent = copy_opponent;
                copy_opponent.step_by_index(opponent_move);

                int move_val = minimax(copy_me, copy_opponent, copy_food_board, depth, INT32_MIN, INT32_MAX, false);

                if (move_val > best_val){
                    best_val = move_val;
                    best_move = my_move;
                }

                // undo opponent;
                copy_opponent = temp_opponent;
            }

            my_move_board ^= my_move;
            // undo my move
            copy_me = temp_me;
        }
        ++depth;
    }

    this->me.step_by_index(best_move);
    // i think the idea is to have the best move be the board position of the head and then calculate action/direction
    return best_move;
}

uint128_t Game::possible_moves(uint128_t head_board) {
    uint16_t head_idx = boost::multiprecision::lsb(head_board);
    uint128_t move_board = uint128_t(0);

    move_board |= uint128_t(1) << (head_idx + 1);
    move_board |= uint128_t(1) << (head_idx - 1);
    move_board |= uint128_t(1) << (head_idx + 11);
    move_board |= uint128_t(1) << (head_idx - 11);

    return move_board;
}

// void Game::step(uint16_t action) {
//     uint128_t new_head = snake_head_board;

//     // left, right, up, down
//     switch (action) {
//         case 0:
//             new_head <<= 1;
//             break;
//         case 1:
//             new_head >>= 1;
//             break;
//         case 2:
//             new_head <<= size;
//             break;
//         default:
//             new_head >>= size;
//             break;
//     }

//     // moved upwards out of bounds 
//     // have to put this first because new_head cannot be 0 when trying to find lsb
//     if (new_head == 0) {
//         done = true;
//         return;
//     }

//     uint16_t idx = boost::multiprecision::lsb(new_head);
//     // std::cout << "index of new head" << idx << std::endl;
//     body_list.push_front(idx);

//     snake_body_board |= snake_head_board;

//     uint128_t old_head = snake_head_board;
//     snake_head_board = new_head;

//     if (snake_head_board & food_board) {
//         get_food();
//         health = 100;
//         score++;
//     } else {
//         health--;
//         uint16_t idx_to_remove = body_list.back();
//         body_list.pop_back();
//         snake_body_board ^= uint128_t(1) << idx_to_remove;
//     }


//     // for some reason, not using the constants is better 

//     if (new_head & snake_body_board) {
//         done = true;
//     } else if (old_head & first_column && new_head & last_column) {
//         done = true;
//     } else if (old_head & last_column && new_head & first_column) {
//         done = true;
//     } else if (old_head & first_row && action == 2) {
//         done = true;
//     } else if (health <= 0) {
//         done = true;
//     }

//     // if (new_head & snake_body_board) {
//     //     done = true;
//     // } else if (old_head & FIRST_COLUMN && action == 0) {
//     //     done = true;
//     // } else if (old_head & LAST_COLUMN && action == 1) {
//     //     done = true;
//     // } else if (old_head & FIRST_ROW && action == 2) {
//     //     done = true;
//     // } else if (health <= 0) {
//     //     done = true;
//     // }

// }

// void Game::get_food() {
//     uint128_t all_boards = (~(snake_body_board | snake_head_board)) ^ OUT_OF_BOUNDS;
    
//     food_choices.clear();
//     // if all the spots are taken up, we can't put any food down 
//     if (!all_boards) {
//         food_board = uint128_t(0);
//     }
//     else {
//         while (all_boards) {
//         uint16_t index = boost::multiprecision::lsb(all_boards);
//         food_choices.push_back(index);
//         all_boards ^= uint128_t(1) << index;
//         }

//         std::random_device rd;
//         std::mt19937 rng(rd());
//         std::uniform_int_distribution<size_t> dist(0, food_choices.size() - 1);
//         size_t random_index = dist(rng);

//         food_board = uint128_t(1) << food_choices[random_index];
//     }
// }

// void Game::reset() {
//     snake_head_board = uint128_t(1) << starting_idx;
//     get_food();
//     score = 0;
//     done = false;
//     health = 100;
//     snake_body_board = uint128_t(0);
//     body_list.clear();
//     body_list.push_back(starting_idx);
// }

// void Game::print_board() const {
//     for (int i = size - 1; i >= 0; --i) {
//         for (int j = size - 1; j >= 0; --j) {
//             uint128_t idx = uint128_t(1) << (i * size + j);
//             if (snake_head_board & idx) {
//                 std::cout << "H ";
//             } else if (food_board & idx) {
//                 std::cout << "F ";
//             } else if (snake_body_board & idx) {
//                 std::cout << "B ";
//             } else {
//                 std::cout << "| ";
//             }
//         }
//         std::cout << std::endl;
//     }
//     std::cout << std::endl;
// }

// void Game::print_individual_board(uint128_t board) const {
//     for (int i = size - 1; i >= 0; --i) {
//         for (int j = size - 1; j >= 0; --j) {
//             uint128_t idx = uint128_t(1) << (i * size + j);
//             if (board & idx) {
//                 std::cout << "1 ";
//             } else {
//                 std::cout << "0 ";
//             }
//         }
//         std::cout << std::endl;
//     }
// }

// void play_game() {
//     Game game(11);
//     game.print_board();
//     uint16_t action;
//     while (true) {
//         std::cin >> action;

//         game.step(action);
//         game.print_board();

//         if (game.done) {
//             std::cout << "DEAD" << std::endl;

//             game.reset();
//             game.print_board();
//         }
//     }
// }

// void run_benchmark() {
//     Game game(11);
//     const int num_games = 100000;
//     std::random_device rd;
//     std::mt19937 rng(rd());
//     std::uniform_int_distribution<uint16_t> dist(0, 3);
//     uint16_t action;

//     auto start = std::chrono::high_resolution_clock::now();

//     for (int i = 0; i < num_games; ++i) {
//         game.reset();
//         while (!game.done) {
//             action = dist(rng);
//             game.step(action);
//         }
//     }

//     auto end = std::chrono::high_resolution_clock::now();
//     auto elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);

//     std::cout << "elapsed time " << elapsed_time.count() << " seconds" << std::endl;
// }


// void test() {
//     Game game(11);
//     std::random_device rd;
//     std::mt19937 rng(rd());
//     std::uniform_int_distribution<uint16_t> dist(0, 3);
//     uint16_t action = 2;

//     int count = 0;
//     while(!game.done) {
//         game.step(action);
//         count++;
//     }
//     std::cout << count << std::endl;

// }

// int main() {
//     run_benchmark();
//     // Game game(11);
//     return 0;
// }