#include "crow.h"
#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include "rapidjson/document.h"

using namespace std;

const uint32_t HASH_SIZE = 4294967295; // 2^32 - 1
const uint16_t ARR_SIZE = 127; // 2^7 - 1 

const uint16_t BOARD_SIZE = 11;

const uint64_t LEFT_COL_FIRSTHALF = 18023198899569664ULL;
const uint64_t LEFT_COL_SECONDHALF = 72092795598278658ULL;
const uint64_t RIGHT_COL_FIRSTHALF = 36046397799139329ULL;
const uint64_t RIGHT_COL_SECONDHALF = 70403120701444ULL;
const uint64_t TOP_ROW = 144044819331678208ULL;
const uint64_t BOTTOM_ROW = 2047ULL;

enum Direction {
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// trying to flatten (3d -> 2d array) yields no results 
vector<uint16_t> precompute_moves[4][121];

void precomp_moves() {
    for (uint16_t direction = LEFT; direction <= DOWN; direction++) {
        for (uint16_t i = 0; i <= 120; i++) {
            switch (direction) {
                case LEFT:
                    if (i >= BOARD_SIZE) precompute_moves[direction][i].push_back(i - BOARD_SIZE);
                    if (i < 120) precompute_moves[direction][i].push_back(i + 1);
                    if (i <= 120 - BOARD_SIZE) precompute_moves[direction][i].push_back(i + BOARD_SIZE);
                    break;
                case RIGHT:
                    if (i >= BOARD_SIZE) precompute_moves[direction][i].push_back(i - BOARD_SIZE);
                    if (i > 0) precompute_moves[direction][i].push_back(i - 1);
                    if (i <= 120 - BOARD_SIZE) precompute_moves[direction][i].push_back(i + BOARD_SIZE);
                    break;
                case UP:
                    if (i > 0) precompute_moves[direction][i].push_back(i - 1);
                    if (i < 120) precompute_moves[direction][i].push_back(i + 1);
                    if (i <= 120 - BOARD_SIZE) precompute_moves[direction][i].push_back(i + BOARD_SIZE);
                    break;
                default:
                    if (i >= BOARD_SIZE) precompute_moves[direction][i].push_back(i - BOARD_SIZE);
                    if (i > 0) precompute_moves[direction][i].push_back(i - 1);
                    if (i < 120) precompute_moves[direction][i].push_back(i + 1);
            }
        }
    }
}


enum Hash_Flag {
    EXACT,
    ALPHA,
    BETA
};


struct tt {
    uint64_t hash_key;
    Hash_Flag flag;
    int depth;
    int value;
};


// struct hash {
//     uint64_t hash_key;
//     // me, opponent, food
//     uint64_t marker_keys[3][121];
//     tt hash_table[HASH_SIZE];

//     hash() {
//         this->hash_key = 0;
//     }

//     void init_random_keys() {
//         random_device rd;
//         mt19937_64 gen(rd());
//         uniform_int_distribution<uint64_t> dis;

//         for (int i = 0; i < 3; i++) {
//             for (int j = 0; j < 121; j++) {
//                 this->marker_keys[i][j] = dis(gen);
//             }
//         }
//     }
    
//     // for starting position 
//     uint64_t generate_hash_key(Player me, Player opponent, uint128_t food_board) {

//     }


//     void clear_hash_table() {
//         for (int i = 0; i < HASH_SIZE; i++) {
//             hash_table[i].hash_key = 0ULL;
//             hash_table[i].flag = EXACT;
//             hash_table[i].depth = 0;
//             hash_table[i].value = 0;
//         }
//     }


//     void write_hash_entry(Hash_Flag flag, int depth, int value) {
//         tt *hash_entry = &hash_table[hash_key & HASH_SIZE];

//         hash_entry->hash_key = hash_key;
//     }


// };

// using size 11 board for offset 
Direction direction_lookup[2 * BOARD_SIZE + 1];

void init_direction_lookup() {
    direction_lookup[1 + BOARD_SIZE] = LEFT;
    direction_lookup[-1 + BOARD_SIZE] = RIGHT;
    direction_lookup[11 + BOARD_SIZE] = UP;
    direction_lookup[-11 + BOARD_SIZE] = DOWN;
}

struct Move {
    uint64_t old_old_head_board_firsthalf, old_old_head_board_secondhalf;
    uint64_t old_head_board_firsthalf, old_head_board_secondhalf;
    uint64_t old_body_board_firsthalf, old_body_board_secondhalf;
    int16_t old_health;
    uint16_t old_length;
    uint16_t old_head_idx, old_tail_idx;
    Direction direction;
    bool old_done, old_just_ate_apple;
    uint64_t old_food_board_firsthalf, old_food_board_secondhalf;
};



struct Player {
    int16_t health;
    uint16_t length;
    Direction direction;

    uint64_t old_head_board_firsthalf;
    uint64_t old_head_board_secondhalf;
    uint64_t snake_head_board_firsthalf;
    uint64_t snake_head_board_secondhalf;
    uint64_t snake_body_board_firsthalf;
    uint64_t snake_body_board_secondhalf;

    uint16_t body_arr[ARR_SIZE] = {0};
    uint16_t head_idx;
    uint16_t tail_idx;

    bool done;
    bool just_ate_apple;


    Player() {}

    Player(uint16_t starting_idx) {
        this->health = 100;
        this->length = 3;
        this->direction = UP;
        this->done = false;
        this->just_ate_apple = false;

        this->head_idx = 2;
        this->tail_idx = 0;
        this->body_arr[this->head_idx] = starting_idx; 

        if (starting_idx < 64) {
            this->snake_head_board_firsthalf = 1ULL << starting_idx;
            this->snake_head_board_secondhalf = 0ULL;
        } 
        else {
            this->snake_head_board_firsthalf = 0ULL;
            this->snake_head_board_secondhalf = 1ULL << (starting_idx & 63);
        }

        this->old_head_board_firsthalf = 0ULL;
        this->old_head_board_secondhalf = 0ULL;
        this->snake_body_board_firsthalf = 0ULL;
        this->snake_body_board_secondhalf = 0ULL;
    }

    Move step_by_index(uint16_t idx) {
        // Construct Move struct with current state
        Move move{
            .old_old_head_board_firsthalf = this->old_head_board_firsthalf,
            .old_old_head_board_secondhalf = this->old_head_board_secondhalf,
            .old_head_board_firsthalf = this->snake_head_board_firsthalf,
            .old_head_board_secondhalf = this->snake_head_board_secondhalf,
            .old_body_board_firsthalf = this->snake_body_board_firsthalf,
            .old_body_board_secondhalf = this->snake_body_board_secondhalf,
            .old_health = this->health,
            .old_length = this->length,
            .old_head_idx = this->head_idx,
            .old_tail_idx = this->tail_idx,
            .direction = this->direction,
            .old_done = this->done,
            .old_just_ate_apple = this->just_ate_apple,
        };

        // Getting new direction before we increment head idx 
        this->direction = direction_lookup[idx - this->body_arr[(this->head_idx) & ARR_SIZE] + BOARD_SIZE];

        this->head_idx++;
        this->body_arr[this->head_idx & ARR_SIZE] = idx;

        // Add current head position to body board
        this->snake_body_board_firsthalf |= this->snake_head_board_firsthalf;
        this->snake_body_board_secondhalf |= this->snake_head_board_secondhalf;

        this->old_head_board_firsthalf = this->snake_head_board_firsthalf;
        this->old_head_board_secondhalf = this->snake_head_board_secondhalf;


        // Set new head position
        if (idx < 64) {
            this->snake_head_board_firsthalf = 1ULL << idx;
            this->snake_head_board_secondhalf = 0ULL;
        }
        else { 
            this->snake_head_board_secondhalf = 1ULL << (idx & 63);
            this->snake_head_board_firsthalf = 0ULL;
        }

        // Update apple_location in Move struct if we just ate an apple
        // if (this->just_ate_apple) {
        //     move.apple_location = idx;
        // }

        return move;
    }
};





struct Game {
    Player me;
    Player opponent;
    uint16_t total_turns;
    uint64_t food_board_firsthalf;
    uint64_t food_board_secondhalf;

    Game() {
        this->total_turns = 0;
        this->food_board_firsthalf = 0ULL;
        this->food_board_secondhalf = 0ULL;
    }

    // prints out me, opponent, and food boards
    void print_board(Player me, Player opponent, uint64_t food_board_firsthalf, uint64_t food_board_secondhalf) const {
        cout << "me length " << me.length << endl;
        cout << "opponent length " << opponent.length << endl;
        for (int i = BOARD_SIZE - 1; i >= 0; --i) {
            for (int j = BOARD_SIZE - 1; j >=0; --j) {
                int idx = i * BOARD_SIZE + j;
                if (idx < 64) {
                    uint64_t val = 1ULL << idx;
                    if (me.snake_head_board_firsthalf & val)
                        cout << "M ";
                    else if (me.snake_body_board_firsthalf & val)
                        cout << "1 ";
                    else if (opponent.snake_head_board_firsthalf & val)
                        cout << "O ";
                    else if (opponent.snake_body_board_firsthalf & val)
                        cout << "2 ";
                    else if (food_board_firsthalf & val)
                        cout << "F ";
                    else
                        cout << "| ";
                }
                else {
                    uint64_t val = 1ULL << (idx & 63);
                    if (me.snake_head_board_secondhalf & val)
                        cout << "M ";
                    else if (me.snake_body_board_secondhalf & val)
                        cout << "1 ";
                    else if (opponent.snake_head_board_secondhalf & val)
                        cout << "O ";
                    else if (opponent.snake_body_board_secondhalf & val)
                        cout << "2 ";
                    else if (food_board_secondhalf & val)
                        cout << "F ";
                    else
                        cout << "| ";
                }
            }
            cout << endl;
        }
        cout << endl;
    }

    void print_individual_board(uint64_t board_firsthalf, uint64_t board_secondhalf) const {
        for (int i = BOARD_SIZE - 1; i >= 0; --i) {
            for (int j = BOARD_SIZE - 1; j >= 0; --j) {
                int idx = i * BOARD_SIZE + j;
                if (idx < 64) {
                    uint64_t val = 1ULL << idx;
                    if (board_firsthalf & val)
                        cout << "1 ";
                    else
                        cout << "| ";
                }
                else {
                    uint64_t val = 1ULL << (idx & 63);
                    if (board_secondhalf & val)
                        cout << "1 ";
                    else
                        cout << "| ";
                }
            }
            cout << endl;
        }
        cout << endl;
    }

    // for the /start endpoint 
    void set_starting_position(uint16_t mysnake_idx, uint16_t opponent_idx, uint64_t food_board_firsthalf, uint64_t food_board_secondhalf) {
        this->me = Player(mysnake_idx);
        this->opponent = Player(opponent_idx);
        this->food_board_firsthalf = food_board_firsthalf;
        this->food_board_secondhalf = food_board_secondhalf;
    }

    // wont get the last couple msb for first 3 moves, but shouldnt matter 
    // need to add num_food_on_board to update_food()
    // need to figure out how to not use lsb function
    // would need to redo this with pairs of 64bit boards
    /*
    void get_food() {
        
        // dont need to xor msb's since free spots only goes up to 120 
        uint128_t all_boards = ~(this->me.snake_body_board | this->me.snake_head_board | this->opponent.snake_body_board | this->opponent.snake_head_board);

        // 0-120 inclusive 
        uint16_t free_spots = 120 - this->num_food_on_board - this->me.length - this->opponent.length;

        // since we know the number of free idxs, we can generate a random number from 0 to free_idx and stop at that number
        random_device rd;
        mt19937 rng(rd());
        uniform_int_distribution<size_t> dist(0, free_spots);
        size_t random_idx = dist(rng);

        // idea is to go up to the random idx without having to go through whole board
        for (int i = 0; i < random_idx; i++) {
            uint16_t idx = boost::multiprecision::lsb(all_boards);
            all_boards ^= uint128_t(1) << idx;
        }

        // can we always guarantee that there will be a place? 
        if (all_boards) {
            uint16_t idx = boost::multiprecision::lsb(all_boards);
            this->food_board |= uint128_t(1) << idx;
            this->num_food_on_board++;
        }
    }
    */

    void update_food(Player &player, uint64_t &food_board_firsthalf, uint64_t &food_board_secondhalf, Move &move) {
        if (player.snake_head_board_firsthalf & food_board_firsthalf || player.snake_body_board_secondhalf & food_board_secondhalf) {
            // remove food from board ; one of the head boards will always be 0
            food_board_firsthalf ^= player.snake_head_board_firsthalf;
            food_board_secondhalf ^= player.snake_head_board_secondhalf;
            
            player.length++;
            // if we didnt eat an apple last move, we remove tail
            if (!player.just_ate_apple) { 
                player.health = 100;

                if (player.body_arr[player.tail_idx & ARR_SIZE] < 64)
                    player.snake_body_board_firsthalf ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];
                else
                    player.snake_body_board_secondhalf ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];
                
                player.tail_idx++;
            }
            player.just_ate_apple = true;
        }
        else {
            player.health--;
            // if we didnt eat an apple last move, we remove tail
            if (!player.just_ate_apple) {
                if (player.body_arr[player.tail_idx & ARR_SIZE] < 64)
                    player.snake_body_board_firsthalf ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];
                else
                    player.snake_body_board_secondhalf ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];

                player.tail_idx++;
            }
            player.just_ate_apple = false;
        }
    }

   // check if we run into wall, ourselves/opponent, or health is 0
   void check_if_done(Player &player, uint64_t all_boards_firsthalf, uint64_t all_boards_secondhalf) {
        if (player.old_head_board_firsthalf) {
            if ((player.old_head_board_firsthalf & LEFT_COL_FIRSTHALF && player.direction == LEFT) ||
                (player.old_head_board_firsthalf & RIGHT_COL_FIRSTHALF && player.direction == RIGHT) ||
                (player.old_head_board_firsthalf & BOTTOM_ROW && player.direction == DOWN) ||
                (player.snake_head_board_firsthalf & all_boards_firsthalf) ||
                (player.snake_head_board_secondhalf & all_boards_secondhalf) || // need to account for head board being in first/second half
                (player.health <= 0)) {
                    player.done = true;
                }
        }
        else {
            if ((player.old_head_board_secondhalf & LEFT_COL_SECONDHALF && player.direction == LEFT) ||
                (player.old_head_board_secondhalf & RIGHT_COL_SECONDHALF && player.direction == RIGHT) ||
                (player.old_head_board_secondhalf & TOP_ROW && player.direction == UP) ||
                (player.snake_head_board_firsthalf & all_boards_firsthalf) ||
                (player.snake_head_board_secondhalf & all_boards_secondhalf) ||
                (player.health <= 0)) {
                    player.done = true;
                }
        }
   }

    // update if we die
    void update_positions(Player &me, Player &opponent) {
        const uint64_t all_boards_firsthalf = me.snake_body_board_firsthalf | opponent.snake_body_board_firsthalf;
        const uint64_t all_boards_secondhalf = me.snake_body_board_secondhalf | opponent.snake_body_board_secondhalf;

        this->check_if_done(me, all_boards_firsthalf, all_boards_secondhalf);
        this->check_if_done(opponent, all_boards_firsthalf, all_boards_secondhalf);

        // need to check if either snake dies before checking if ran into head; 
        // ex. ran into wall wraps around to other side which could kill other snake
        if (me.done || opponent.done)
            return;

        // if either runs into others head, check which would win 
        if (me.snake_head_board_firsthalf & opponent.snake_head_board_firsthalf || me.snake_body_board_secondhalf & opponent.snake_head_board_secondhalf) {
            if (me.length > opponent.length)
                opponent.done = true;
            else if (me.length < opponent.length)
                me.done = true;
            else {
                me.done = true;
                opponent.done = true;
            }
        }
    }


    void undo_move(Player &player, Move &move, uint64_t &food_board_firsthalf, uint64_t &food_board_secondhalf) {
        
        // if the new move just ate an apple, we need to remove it 
        // if (player.just_ate_apple) {
        //     food_board_firsthalf |= player.snake_head_board_firsthalf;
        //     food_board_secondhalf |= player.snake_head_board_secondhalf;
        // }

        // revert food board
        food_board_firsthalf = move.old_food_board_firsthalf;
        food_board_secondhalf = move.old_food_board_secondhalf;

        // revert head position 
        player.snake_head_board_firsthalf = move.old_head_board_firsthalf;
        player.snake_head_board_secondhalf = move.old_head_board_secondhalf;

        // revert body board
        player.snake_body_board_firsthalf = move.old_body_board_firsthalf;
        player.snake_body_board_secondhalf = move.old_body_board_secondhalf;

        // revert old head board
        player.old_head_board_firsthalf = move.old_old_head_board_firsthalf;
        player.old_head_board_secondhalf = move.old_old_head_board_secondhalf;

        // revert index and direction
        player.head_idx = move.old_head_idx;
        player.tail_idx = move.old_tail_idx;
        player.direction = move.direction;

        player.length = move.old_length;
        player.just_ate_apple = move.old_just_ate_apple;
        player.done = move.old_done; // believe we can just remove it bc minimax takes care of done positions 
        player.health = move.old_health;
    }

    int evaluate(Player &me, Player &opponent, int &depth) const {

        int score = 0;

        int my_score = 0;
        int opponent_score = 0;

        if (me.done) {
            my_score += -1000 + depth;
        }
        else if (me.just_ate_apple) {
            my_score += 100 - depth;
        }

        if (opponent.done) {
            opponent_score += 1000 - depth;
        }
        else if (opponent.just_ate_apple) {
            opponent_score += -100 + depth;
        }

        score += my_score;
        score += opponent_score;

        return score;
    }

    int minimax(Player &me, Player &opponent, uint64_t &food_board_firsthalf, uint64_t &food_board_secondhalf, int depth, int alpha, int beta, int &nodes_visited) {

        // cout << "head " << me.snake_head_board_firsthalf << " " << me.snake_head_board_secondhalf << endl;
        // cout << "body " << me.snake_body_board_firsthalf << " " << me.snake_body_board_secondhalf << endl;
        // cout << "old head " << me.old_head_board_firsthalf << " " << me.old_head_board_secondhalf << endl;
        // cout << "head idx " << me.head_idx << endl;
        // cout << "tail idx " << me.tail_idx << endl;
        // cout << "direction " << me.direction << endl;
        // cout << "food " << food_board_firsthalf << " " << food_board_secondhalf << endl;
        // cout << endl;


        if (depth == 0 || me.done || opponent.done){
            if (me.done && opponent.done)
                return -1000 + depth;
            if (me.just_ate_apple && opponent.just_ate_apple)
                return 100 - depth;

            return evaluate(me, opponent, depth);
        }

        int best_max_score = INT32_MIN;
        int best_min_score = INT32_MAX;

        const vector<uint16_t>& my_move_board = precompute_moves[me.direction][me.body_arr[me.head_idx & ARR_SIZE]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.direction][opponent.body_arr[opponent.head_idx & ARR_SIZE]];

        for (const uint16_t my_move : my_move_board) {
            Move my_move_struct = me.step_by_index(my_move);
            my_move_struct.old_food_board_firsthalf = food_board_firsthalf;
            my_move_struct.old_food_board_secondhalf = food_board_secondhalf;
            update_food(me, food_board_firsthalf, food_board_secondhalf, my_move_struct);

            for (const uint16_t opponent_move : opponent_move_board) {
                
                Move opponent_move_struct = opponent.step_by_index(opponent_move);
                opponent_move_struct.old_food_board_firsthalf = food_board_firsthalf;
                opponent_move_struct.old_food_board_secondhalf = food_board_secondhalf;
                update_food(opponent, food_board_firsthalf, food_board_secondhalf, opponent_move_struct);

                update_positions(me, opponent);
                nodes_visited++;
                int score = minimax(me, opponent, food_board_firsthalf, food_board_secondhalf, depth - 1, alpha, beta, nodes_visited);

                undo_move(opponent, opponent_move_struct, food_board_firsthalf, food_board_secondhalf); // Undo opponent's move

                best_min_score = min(best_min_score, score);
                best_max_score = max(best_max_score, score);
                alpha = max(alpha, best_max_score);
                beta = min(beta, best_min_score);

                if (alpha >= beta){
                    break;
                }
            }
            undo_move(me, my_move_struct, food_board_firsthalf, food_board_secondhalf); // Undo 'me's move
        }
        return best_max_score;
    }

    uint16_t find_best_move(Player me, Player opponent, uint64_t food_board_firsthalf, uint64_t food_board_secondhalf, int max_depth) {

        int best_score = INT32_MIN;
        uint16_t best_move;

        int depth = 0;
        auto start_time = chrono::high_resolution_clock::now();
        int nodes_visited = 0;

        const vector<uint16_t>& my_move_board = precompute_moves[me.direction][me.body_arr[me.head_idx & ARR_SIZE]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.direction][opponent.body_arr[opponent.head_idx & ARR_SIZE]];

        while (depth < max_depth) {
            for (const uint16_t my_move : my_move_board) {
                Move my_move_struct = me.step_by_index(my_move);
                update_food(me, food_board_firsthalf, food_board_secondhalf, my_move_struct);

                for (const uint16_t opponent_move : opponent_move_board) {
                    Move opponent_move_struct = opponent.step_by_index(opponent_move);
                    update_food(opponent, food_board_firsthalf, food_board_secondhalf, opponent_move_struct);
                    
                    update_positions(me, opponent);
                    nodes_visited++;

                    int score = minimax(me, opponent, food_board_firsthalf, food_board_secondhalf, depth, INT32_MIN, INT32_MAX, nodes_visited);
                    
                    undo_move(opponent, opponent_move_struct, food_board_firsthalf, food_board_secondhalf); // Undo opponent's move
                    
                    if (score > best_score) {
                        best_score = score;
                        best_move = my_move;
                    }
                }
                undo_move(me, my_move_struct, food_board_firsthalf, food_board_secondhalf); // Undo 'me's move
            }
            depth++;
        }
        cout << "nodes visited " << nodes_visited << endl;
        return best_move;
    }
};


void testing() {
    Game game;

    uint16_t my_starting_idx = 61; // 12 // 61
    uint16_t opponent_starting_idx = 59; // 20
    
    uint16_t food[] = {8, 2, 60, 83, 81};
    uint64_t food_board_firsthalf = 0ULL;
    uint64_t food_board_secondhalf = 0ULL;

    for (uint16_t f : food) {
        if (f < 64)
            food_board_firsthalf |= 1ULL << f;
        else
            food_board_secondhalf |= 1ULL << (f & 63);
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board_firsthalf, food_board_secondhalf);
    game.print_board(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf);

    // for (int i = 1; i < 6; i++) {
    //     game.me.step_by_index(61 + (11 * i));
    //     game.update_food(game.me, game.food_board);

    //     game.opponent.step_by_index(59 + (11 * i));
    //     game.update_food(game.opponent, game.food_board);
        
    //     game.update_positions(game.me, game.opponent);

    //     game.print_board(game.me, game.opponent, game.food_board);
    // }


    // uint16_t move = game.find_best_move(game.me, game.opponent, game.food_board, 2);
    // game.me.step_by_index(move);

}


void benchmark() {
    Game game;

    uint16_t my_starting_idx = 12;
    uint16_t opponent_starting_idx = 20; 
    
    uint16_t food[] = {8, 2, 60};

    uint64_t food_board_firsthalf = 0ULL;
    uint64_t food_board_secondhalf = 0ULL;

    for (uint16_t f : food) {
        if (f < 64)
            food_board_firsthalf |= 1ULL << f;
        else
            food_board_secondhalf |= 1ULL << (f & 63);
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board_firsthalf, food_board_secondhalf);
    // game.print_board(game.me, game.opponent, game.food_board);

    auto start_time = chrono::high_resolution_clock::now();
    uint16_t move = game.find_best_move(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf, 15);

    // auto end_time = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count();
    auto end_time = chrono::high_resolution_clock::now();
    double time_taken = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
    cout << "total time: " << time_taken << endl;
    
}


int main()
{

    precomp_moves();
    init_direction_lookup();

    // testing();
    // exit(0);

    benchmark();
    exit(0);


    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

    Game game;
    rapidjson::Document doc;

    uint16_t my_prev_move;

    CROW_ROUTE(app, "/")([](){
        crow::json::wvalue data;
        data["apiversion"] = "1";
        data["author"] = "";
        data["color"] = "#888888";
        data["head"] = "default";
        data["tail"] = "default";

        return data;
    });



    CROW_ROUTE(app, "/start")
    .methods("POST"_method)
    ([&](const crow::request& req){

        uint16_t my_starting_idx, opponent_starting_idx;
        game = Game();
        // rapidjson::Document doc;
        doc.Parse(req.body.c_str());

        // Get your snake's starting head index
        my_starting_idx = 11 * doc["you"]["head"]["y"].GetInt() + (11 - doc["you"]["head"]["x"].GetInt()) - 1;

        // Get opponents' starting head index
        const auto& snakes = doc["board"]["snakes"];
        for (rapidjson::SizeType i = 0; i < snakes.Size(); i++) {
            const auto& snake = snakes[i];
            if (snake["id"] != doc["you"]["id"]) { // If not your snake`
                opponent_starting_idx = 11 * snake["head"]["y"].GetInt() + (11 - snake["head"]["x"].GetInt()) - 1;
                break;
            }
        }

        // maybe just compute the food board instead of using array/vector
        const auto& food = doc["board"]["food"];
        
        uint64_t food_board_firsthalf = 0ULL;
        uint64_t food_board_secondhalf = 0ULL;

        for (rapidjson::SizeType i = 0; i < food.Size(); i++) {
            int val = (11 * food[i]["y"].GetInt() + (11 - food[i]["x"].GetInt()) - 1);
            if (val < 64)
                food_board_firsthalf |= 1ULL << val;
            else
                food_board_secondhalf |= 1ULL << (val & 63);
        }

        game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board_firsthalf, food_board_secondhalf);

        cout << "Starting" << endl;
        return "";
    });

    

    CROW_ROUTE(app, "/move")
    .methods("POST"_method)
    ([&](const crow::request& req){
        doc.Parse(req.body.c_str());

        uint16_t turn = doc["turn"].GetInt();
        // cout << "turn " << turn << endl;
        
        game.total_turns = turn;

        uint16_t opponent_idx;

        const auto& snakes = doc["board"]["snakes"];
        for (rapidjson::SizeType i = 0; i < snakes.Size(); i++) {
            const auto& snake = snakes[i];
            if (snake["id"] != doc["you"]["id"]) { // If not your snake
                opponent_idx = 11 * snake["head"]["y"].GetInt() + (11 - snake["head"]["x"].GetInt()) - 1;
                break;
            }
        }

        const auto& food = doc["board"]["food"];
        
        uint64_t food_board_firsthalf = 0ULL;
        uint64_t food_board_secondhalf = 0ULL;

        for (rapidjson::SizeType i = 0; i < food.Size(); i++) {
            int val = (11 * food[i]["y"].GetInt() + (11 - food[i]["x"].GetInt()) - 1);
            if (val < 64)
                food_board_firsthalf |= 1ULL << val;
            else
                food_board_secondhalf |= 1ULL << (val & 63);
        }

        //should try and update opponent move and my previous move that i sent to the server 
        if (turn != 0){
            // exit(0);
            // has to be before updating food
            Move me_move_struct = game.me.step_by_index(my_prev_move);
            Move opp_move_struct = game.opponent.step_by_index(opponent_idx);
            game.update_food(game.me, game.food_board_firsthalf, game.food_board_secondhalf, me_move_struct);
            game.update_food(game.opponent, game.food_board_firsthalf, game.food_board_secondhalf, opp_move_struct);
            game.update_positions(game.me, game.opponent);

            game.food_board_firsthalf = food_board_firsthalf;
            game.food_board_secondhalf = food_board_secondhalf;
        }

        cout << "turn " <<  turn << endl;
        game.print_board(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf);

        // getting the best move 
        uint16_t move_idx = game.find_best_move(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf, 50);
        my_prev_move = move_idx; // will actually move the snake later with opponent 
        // get the direction which ai picked from the index 
        int dir = direction_lookup[move_idx - game.me.body_arr[(game.me.head_idx) & ARR_SIZE] + 11];
        // cout << "Move is " << dir << endl;

        crow::json::wvalue data;
        switch(dir) {
            case 1:
                data["move"] = "left";
                break;
            case -1:
                data["move"] = "right";
                break;
            case 11:
                data["move"] = "up";
                break;
            default:
                data["move"] = "down";
        }

        return data;
    });



    CROW_ROUTE(app, "/end")
    .methods("POST"_method)
    ([&](const crow::request& req){

        uint16_t opponent_idx;

        const auto& snakes = doc["board"]["snakes"];
        for (rapidjson::SizeType i = 0; i < snakes.Size(); i++) {
            const auto& snake = snakes[i];
            if (snake["id"] != doc["you"]["id"]) { // If not your snake
                opponent_idx = 11 * snake["head"]["y"].GetInt() + (11 - snake["head"]["x"].GetInt()) - 1;
                break;
            }
        }

        const auto& food = doc["board"]["food"];
        
        uint64_t food_board_firsthalf = 0ULL;
        uint64_t food_board_secondhalf = 0ULL;

        for (rapidjson::SizeType i = 0; i < food.Size(); i++) {
            int val = (11 * food[i]["y"].GetInt() + (11 - food[i]["x"].GetInt()) - 1);
            if (val < 64)
                food_board_firsthalf |= 1ULL << val;
            else
                food_board_secondhalf |= 1ULL << (val & 63);
        }
        
        
        uint64_t temp_food_firsthalf = food_board_firsthalf;
        uint64_t temp_food_secondhalf = food_board_secondhalf;
        Move me_move_struct = game.me.step_by_index(my_prev_move);
        Move opp_move_struct = game.opponent.step_by_index(opponent_idx);
        game.update_food(game.me, temp_food_firsthalf, temp_food_secondhalf, me_move_struct);

        temp_food_firsthalf = food_board_firsthalf;
        temp_food_secondhalf = food_board_secondhalf;

        game.update_food(game.opponent, temp_food_firsthalf, temp_food_secondhalf, opp_move_struct);
        game.update_positions(game.me, game.opponent);

        game.food_board_firsthalf = food_board_firsthalf;
        game.food_board_secondhalf = food_board_secondhalf;

        game.print_board(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf);
        cout << "ENDED" << endl;
        // game = Game(); // reset game 
        return "";
    });

    // app.port(8080).run();

    app.port(8080).multithreaded().run();

}