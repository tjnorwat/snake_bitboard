#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>

#define uint128_t __uint128_t
#define ONE_128 ((__uint128_t)1)
#define ZERO_128 ((__uint128_t)0)

using namespace std;

static constexpr uint16_t ARR_SIZE = 127; // 2^7 - 1 

static constexpr uint16_t BOARD_SIZE = 11;

enum Direction {
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// precomputed wall collisions for each direction and index of previous snake head
bool precompute_wall_collisions[4][BOARD_SIZE * BOARD_SIZE];
void precomp_wall_collisions() {
    for (uint16_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
        int inverted_index = BOARD_SIZE * BOARD_SIZE - 1 - i; // Invert index to match LSB to MSB representation
        int row = inverted_index / BOARD_SIZE;
        int col = inverted_index % BOARD_SIZE;

        precompute_wall_collisions[LEFT][i] = (col == 0);
        precompute_wall_collisions[RIGHT][i] = (col == BOARD_SIZE - 1);
        precompute_wall_collisions[UP][i] = (row == 0);
        precompute_wall_collisions[DOWN][i] = (row == BOARD_SIZE - 1);
    }
}

// trying to flatten (3d -> 2d array) yields no results 
vector<uint16_t> precompute_moves[4][BOARD_SIZE * BOARD_SIZE];
// array of precomputed moves based on direction and index of snake head
// number of moves can vary based on where snake is on the board ex. top left corner/ bottom right corner
void precomp_moves() {
    for (uint16_t direction = LEFT; direction <= DOWN; direction++) {
        for (uint16_t i = 0; i < 121; i++) {
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
                    break;
            }
        }
    }
}

// using size 11 board for offset 
Direction direction_lookup[2 * BOARD_SIZE + 1];

void init_direction_lookup() {
    direction_lookup[1 + BOARD_SIZE] = LEFT;
    direction_lookup[-1 + BOARD_SIZE] = RIGHT;
    direction_lookup[BOARD_SIZE + BOARD_SIZE] = UP;
    direction_lookup[-BOARD_SIZE + BOARD_SIZE] = DOWN;
}

struct alignas(64) Player {
    int16_t health;
    uint16_t length;
    Direction direction;

    uint128_t snake_head_board;
    uint128_t snake_body_board;

    uint16_t body_arr[ARR_SIZE] = {0};
    uint16_t head_idx;
    uint16_t tail_idx;

    bool done;
    bool just_ate_apple;


    Player() {}

    Player(uint16_t starting_idx) : health(100), length(3), direction(UP), snake_head_board(ZERO_128),
        snake_body_board(ZERO_128), head_idx(2), tail_idx(0), done(false), just_ate_apple(false) {

        // snake is 3 long at the start but only occupies 1 spot on the board. 
        // to make it 3 long, we offset the head idx by 2
        // whenever we call the update food function and xor the tail with the body, it will 'mess up' the first call but then correct itself afterwards 
        this->body_arr[this->head_idx] = starting_idx; 


        this->snake_head_board = ONE_128 << starting_idx;
    }

    void step_by_index(const uint16_t &idx) {
        // getting new direction before we increment head idx 
        // unordered map and switch statement are worse
        this->direction = direction_lookup[idx - this->body_arr[(this->head_idx) & ARR_SIZE] + BOARD_SIZE]; 

        // Add current head position to body board
        this->snake_body_board |= this->snake_head_board;

        // Set new head position
        // always will have one of the two set to 0
        this->snake_head_board = ONE_128 << idx;

        this->head_idx++;
        this->body_arr[this->head_idx & ARR_SIZE] = idx;
    }

    void step_by_direction(Direction dir) {
        // getting new index before we increment head idx
        uint16_t current_idx = this->body_arr[this->head_idx & ARR_SIZE];
        uint16_t new_idx;

        switch (dir) {
            case LEFT:
                new_idx = (current_idx % BOARD_SIZE == 0) ? current_idx : current_idx + 1;
                break;
            case RIGHT:
                new_idx = (current_idx % BOARD_SIZE == BOARD_SIZE - 1) ? current_idx : current_idx - 1;
                break;
            case UP:
                new_idx = (current_idx < BOARD_SIZE) ? current_idx : current_idx + BOARD_SIZE;
                break;
            default:
                new_idx = (current_idx >= 121 - BOARD_SIZE) ? current_idx : current_idx - BOARD_SIZE;
                break;
        }

        this->direction = dir;
        this->head_idx++;
        this->body_arr[this->head_idx & ARR_SIZE] = new_idx;

        // // Add current head position to body board
        // this->snake_body_board_firsthalf |= this->snake_head_board_firsthalf;
        // this->snake_body_board_secondhalf |= this->snake_head_board_secondhalf;

        // // Set new head position
        // // always will have one of the two set to 0
        // if (new_idx < 64) {
        //     this->snake_head_board_firsthalf = 1ULL << new_idx;
        //     this->snake_head_board_secondhalf = 0ULL;
        // }
        // else {
        //     this->snake_head_board_secondhalf = 1ULL << (new_idx & 63);
        //     this->snake_head_board_firsthalf = 0ULL;
        // }
    }
};


struct Game {
    Player me;
    Player opponent;
    uint16_t total_turns;
    uint128_t food_board;

    Game(): total_turns(0), food_board(ZERO_128) {}

    // for the /start endpoint 
    void set_starting_position(uint16_t mysnake_idx, uint16_t opponent_idx, uint128_t food_board) {
        this->me = Player(mysnake_idx);
        this->opponent = Player(opponent_idx);
        this->food_board = food_board;
    }

    void update_food(Player &player, uint128_t &food_board) {
        if (player.snake_head_board & food_board) {
            // remove food from board ; one of the head boards will always be 0
            food_board ^= player.snake_head_board;
            
            player.length++;
            // if we didnt eat an apple last move, we remove tail
            if (!player.just_ate_apple) { 
                player.health = 100;

                player.snake_body_board ^= ONE_128 << player.body_arr[player.tail_idx & ARR_SIZE];
                
                player.tail_idx++;
            }
            player.just_ate_apple = true;
        }
        else {
            player.health--;
            // if we didnt eat an apple last move, we remove tail
            if (!player.just_ate_apple) {
                player.snake_body_board ^= ONE_128 << player.body_arr[player.tail_idx & ARR_SIZE];

                player.tail_idx++;
            }
            player.just_ate_apple = false;
        }
    }

    // update if we die
    void update_positions(Player &me, Player &opponent) {

        // have to do wall collisions first because if we run into wall and opponent runs into our body, only we die 
        // ex. ran into wall wraps around to other side which could kill other snake
        if (precompute_wall_collisions[me.direction][me.body_arr[(me.head_idx - 1) & ARR_SIZE]])
            me.done = true;

        if (precompute_wall_collisions[opponent.direction][opponent.body_arr[(opponent.head_idx - 1) & ARR_SIZE]])
            opponent.done = true;

        // have to return only after wall collisions (that i know of)
        if (me.done || opponent.done)
            return;

        const uint128_t all_boards = me.snake_body_board | opponent.snake_body_board;

        if (me.snake_head_board & all_boards)
            me.done = true;
        else if (me.health <= 0) 
            me.done = true;
        

        if (opponent.snake_head_board & all_boards) 
            opponent.done = true;
        else if (opponent.health <= 0) 
            opponent.done = true;
        
        
        // if either runs into others head, check which would win 
        if (me.snake_head_board & opponent.snake_head_board) {
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

    int evaluate(const Player &me, const Player &opponent, const int &depth) const {

        int score = 0;

        // int my_score = 0;
        // int opponent_score = 0;

        // if (me.done) {
        //     my_score += -1000 + depth;
        // }
        // else if (me.just_ate_apple) {
        //     my_score += 100 - depth;
        // }

        // if (opponent.done) {
        //     opponent_score += 1000 - depth;
        // }
        // else if (opponent.just_ate_apple) {
        //     opponent_score += -100 + depth;
        // }

        // score += my_score;
        // score += opponent_score;

        return score;
    }

    int minimax(Player &me, Player &opponent, const uint128_t food_board, int depth, int alpha, int beta, int &nodes_visited) {
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

        for (const uint16_t& my_move : my_move_board) {
            uint128_t my_food_board = food_board;

            Player temp_me = me;
            temp_me.step_by_index(my_move);
            update_food(temp_me, my_food_board);

            for (const uint16_t& opponent_move : opponent_move_board) {
                uint128_t opponent_food_board = food_board;

                Player temp_opponent = opponent;
                temp_opponent.step_by_index(opponent_move);
                update_food(temp_opponent, opponent_food_board);

                update_positions(temp_me, temp_opponent);
                nodes_visited++;
                int score = minimax(temp_me, temp_opponent, my_food_board & opponent_food_board, depth - 1, alpha, beta, nodes_visited);
                best_min_score = min(best_min_score, score);
                best_max_score = max(best_max_score, score);
                alpha = max(alpha, best_max_score);
                beta = min(beta, best_min_score);

                if (alpha >= beta)
                    break;
            }
        }
        return best_max_score;
    }

    uint16_t find_best_move(Player me, Player opponent, uint128_t food_board_firsthalf, int max_depth) {

        int best_score = INT32_MIN;
        uint16_t best_move;

        int depth = 0;
        auto start_time = chrono::high_resolution_clock::now();
        int nodes_visited = 0;

        const vector<uint16_t>& my_move_board = precompute_moves[me.direction][me.body_arr[me.head_idx & ARR_SIZE]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.direction][opponent.body_arr[opponent.head_idx & ARR_SIZE]];

        // while (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count() < 50 && depth < max_depth) {
        while (depth < max_depth) {
            for (const uint16_t& my_move : my_move_board) {
                uint128_t my_food_board = food_board;

                Player temp_me = me;
                temp_me.step_by_index(my_move);
                update_food(temp_me, my_food_board);

                for (const uint16_t& opponent_move : opponent_move_board) {
                    uint128_t opponent_food_board = food_board;

                    Player temp_opponent = opponent;
                    temp_opponent.step_by_index(opponent_move);
                    update_food(temp_opponent, opponent_food_board);
                    update_positions(temp_me, temp_opponent);
                    nodes_visited++;
                    int score = minimax(temp_me, temp_opponent, my_food_board & opponent_food_board, depth, INT32_MIN, INT32_MAX, nodes_visited);
                    
                    if (score > best_score) {
                        best_score = score;
                        best_move = my_move;
                    }
                }
            }
            depth++;
        }
        cout << "nodes visited " << nodes_visited << endl;
        return best_move;
    }

    // prints out me, opponent, and food boards
    // void print_board(Player me, Player opponent, uint64_t food_board_firsthalf, uint64_t food_board_secondhalf) const {
    //     cout << "me length " << me.length << endl;
    //     cout << "opponent length " << opponent.length << endl;
    //     for (int i = BOARD_SIZE - 1; i >= 0; --i) {
    //         for (int j = BOARD_SIZE - 1; j >=0; --j) {
    //             int idx = i * BOARD_SIZE + j;
    //             if (idx < 64) {
    //                 uint64_t val = 1ULL << idx;
    //                 if (me.snake_head_board_firsthalf & val)
    //                     cout << "M ";
    //                 else if (me.snake_body_board_firsthalf & val)
    //                     cout << "1 ";
    //                 else if (opponent.snake_head_board_firsthalf & val)
    //                     cout << "O ";
    //                 else if (opponent.snake_body_board_firsthalf & val)
    //                     cout << "2 ";
    //                 else if (food_board_firsthalf & val)
    //                     cout << "F ";
    //                 else
    //                     cout << "| ";
    //             }
    //             else {
    //                 uint64_t val = 1ULL << (idx & 63);
    //                 if (me.snake_head_board_secondhalf & val)
    //                     cout << "M ";
    //                 else if (me.snake_body_board_secondhalf & val)
    //                     cout << "1 ";
    //                 else if (opponent.snake_head_board_secondhalf & val)
    //                     cout << "O ";
    //                 else if (opponent.snake_body_board_secondhalf & val)
    //                     cout << "2 ";
    //                 else if (food_board_secondhalf & val)
    //                     cout << "F ";
    //                 else
    //                     cout << "| ";
    //             }
    //         }
    //         cout << endl;
    //     }
    //     cout << endl;
    // }

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

    // void print_player_board(Player player) const {
    //     cout << "player length " << player.length << endl;
    //     for (int i = BOARD_SIZE - 1; i >= 0; --i) {
    //         for (int j = BOARD_SIZE - 1; j >= 0; --j) {
    //             int idx = i * BOARD_SIZE + j;
    //             if (idx < 64) {
    //                 uint64_t val = 1ULL << idx;
    //                 if (player.snake_head_board_firsthalf & val)
    //                     cout << "H ";
    //                 else if (player.snake_body_board_firsthalf & val)
    //                     cout << "B ";
    //                 else
    //                     cout << "| ";
    //             }
    //             else {
    //                 uint64_t val = 1ULL << (idx & 63);
    //                 if (player.snake_head_board_secondhalf & val)
    //                     cout << "H ";
    //                 else if (player.snake_body_board_secondhalf & val)
    //                     cout << "B ";
    //                 else
    //                     cout << "| ";
    //             }
    //         }
    //         cout << endl;
    //     }
    //     cout << endl;
    // }
};

void benchmark() {
    Game game;
    uint16_t my_starting_idx = 12;
    uint16_t opponent_starting_idx = 20; 
    
    uint16_t food[] = {8, 2, 60};

    uint128_t food_board = ZERO_128;
    uint64_t food_board_firsthalf = 0ULL;
    uint64_t food_board_secondhalf = 0ULL;

    for (uint16_t f : food) {
        food_board |= ONE_128 << f;
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);
    // game.print_board(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf);

    auto start_time = chrono::high_resolution_clock::now();
    uint16_t move = game.find_best_move(game.me, game.opponent, game.food_board, 15);

    auto end_time = chrono::high_resolution_clock::now();
    double time_taken = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
    cout << "total time: " << time_taken << endl;
}


int main() {

    precomp_moves();
    precomp_wall_collisions();
    init_direction_lookup();

    benchmark();

    return 0;
}