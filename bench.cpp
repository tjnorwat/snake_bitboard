#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <utility>

using namespace std;

static constexpr uint16_t ARR_SIZE = 127; // 2^7 - 1 
static constexpr uint16_t BOARD_SIZE = 11;


inline uint64_t lsb(uint64_t x) {
    return __builtin_ctzll(x);
}

inline uint64_t popLSB(uint64_t *x) {
    uint64_t l = lsb(*x);
    *x &= *x - 1;
    return l;
}


vector<uint16_t> precompute_moves[BOARD_SIZE * BOARD_SIZE];
// array of precomputed moves based on direction and index of snake head
// number of moves can vary based on where snake is on the board ex. top left corner/ bottom right corner
void precomp_moves() {
    for (uint16_t i = 0; i < 121; i++) {
        if ((i + 1) % BOARD_SIZE != 0) // Prevent wrapping to the next row left 
            precompute_moves[i].push_back(i + 1);
        if (i % BOARD_SIZE != 0) // Prevent wrapping to the previous row right 
            precompute_moves[i].push_back(i - 1);
        if (i < 121 - BOARD_SIZE) // Moving up
            precompute_moves[i].push_back(i + BOARD_SIZE);
        if (i >= BOARD_SIZE) // Moving down
            precompute_moves[i].push_back(i - BOARD_SIZE);
    }
}


struct alignas(64) Player {
    int16_t health;
    uint16_t length;

    pair<uint64_t, uint64_t> old_head_board;
    pair<uint64_t, uint64_t> snake_head_board;
    pair<uint64_t, uint64_t> snake_body_board;

    uint16_t body_arr[ARR_SIZE] = {0};
    uint16_t head_idx;
    uint16_t tail_idx;

    bool done;
    bool just_ate_apple;

    Player() : health(100), length(3), old_head_board(0ULL, 0ULL), snake_head_board(0ULL, 0ULL),
               snake_body_board(0ULL, 0ULL), head_idx(2), tail_idx(0), done(false), just_ate_apple(false) {}

    Player(uint16_t starting_idx) : Player() {
        // Initialize player position
        set_head_position(starting_idx);
    }

    void set_head_position(uint16_t starting_idx) {
        this->body_arr[this->head_idx] = starting_idx; 

        if (starting_idx < 64) {
            this->snake_head_board.first = 1ULL << starting_idx;
        } else {
            this->snake_head_board.second = 1ULL << (starting_idx & 63);
        }
    }

    void step_by_index(const uint16_t &idx) {
        // Save current head position
        this->old_head_board = this->snake_head_board;

        // Add current head position to body board
        this->snake_body_board.first |= this->snake_head_board.first;
        this->snake_body_board.second |= this->snake_head_board.second;

        // Set new head position
        if (idx < 64) {
            this->snake_head_board = {1ULL << idx, 0ULL};
        } else {
            this->snake_head_board = {0ULL, 1ULL << (idx & 63)};
        }

        this->head_idx++;
        this->body_arr[this->head_idx & ARR_SIZE] = idx;
    }
};


struct Game {
    Player me;
    Player opponent;
    uint16_t total_turns;
    pair<uint64_t, uint64_t> food_board;

    Game(): total_turns(0), food_board(0ULL, 0ULL) {}

    // for the /start endpoint 
    void set_starting_position(uint16_t mysnake_idx, uint16_t opponent_idx, pair<uint64_t, uint64_t> food_board) {
        this->me = Player(mysnake_idx);
        this->opponent = Player(opponent_idx);
        this->food_board = {food_board.first, food_board.second};
    }

    void update_food(Player &player, pair<uint64_t, uint64_t> &food_board) {
        if (player.snake_head_board.first & food_board.first || player.snake_head_board.second & food_board.second) {
            // remove food from board ; one of the head boards will always be 0
            food_board.first ^= player.snake_head_board.first;
            food_board.second ^= player.snake_head_board.second;
            
            player.length++;
            // if we didnt eat an apple last move, we remove tail
            if (!player.just_ate_apple) { 
                player.health = 100;

                if (player.body_arr[player.tail_idx & ARR_SIZE] < 64)
                    player.snake_body_board.first ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];
                else
                    player.snake_body_board.second ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];
                
                player.tail_idx++;
            }
            player.just_ate_apple = true;
        }
        else {
            player.health--;
            // if we didnt eat an apple last move, we remove tail
            if (!player.just_ate_apple) {
                if (player.body_arr[player.tail_idx & ARR_SIZE] < 64)
                    player.snake_body_board.first ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];
                else
                    player.snake_body_board.second ^= 1ULL << player.body_arr[player.tail_idx & ARR_SIZE];

                player.tail_idx++;
            }
            player.just_ate_apple = false;
        }
    }

    // update if we die
    void update_positions(Player &me, Player &opponent) {

        const pair<uint64_t, uint64_t> all_boards = {me.snake_body_board.first | opponent.snake_body_board.first, me.snake_body_board.second | opponent.snake_body_board.second};

        if (me.snake_head_board.first & all_boards.first || me.snake_head_board.second & all_boards.second)
            me.done = true;
        else if (me.health <= 0) 
            me.done = true;
        

        if (opponent.snake_head_board.first & all_boards.first || opponent.snake_head_board.second & all_boards.second) 
            opponent.done = true;
        else if (opponent.health <= 0) 
            opponent.done = true;
        
        // if either runs into others head, check which would win 
        if (me.snake_head_board.first & opponent.snake_head_board.first || me.snake_body_board.second & opponent.snake_head_board.second) {
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
        return score;
    }

    int minimax(Player &me, Player &opponent, pair<uint64_t, uint64_t> &food_board, int depth, int alpha, int beta, int &nodes_visited) {
        if (depth == 0 || me.done || opponent.done){
            // if (me.done && opponent.done)
            //     return -1000 + depth;
            // if (me.just_ate_apple && opponent.just_ate_apple)
            //     return 100 - depth;

            return evaluate(me, opponent, depth);
        }

        int best_max_score = INT32_MIN;
        int best_min_score = INT32_MAX;

        const vector<uint16_t>& my_move_board = precompute_moves[me.body_arr[me.head_idx & ARR_SIZE]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.body_arr[opponent.head_idx & ARR_SIZE]];

        for (const uint16_t& my_move : my_move_board) {
            pair<uint64_t, uint64_t> my_food_board = food_board;

            Player temp_me = me;
            temp_me.step_by_index(my_move);
            update_food(temp_me, my_food_board);

            for (const uint16_t& opponent_move : opponent_move_board) {
                pair<uint64_t, uint64_t> opponent_food_board = food_board;

                Player temp_opponent = opponent;
                temp_opponent.step_by_index(opponent_move);
                update_food(temp_opponent, opponent_food_board);

                update_positions(temp_me, temp_opponent);
                nodes_visited++;

                pair<uint64_t, uint64_t> new_food = {my_food_board.first & opponent_food_board.first, my_food_board.second & opponent_food_board.second};
                
                int score = minimax(temp_me, temp_opponent, new_food, depth - 1, alpha, beta, nodes_visited);
                best_min_score = min(best_min_score, score);
                best_max_score = max(best_max_score, score);
                alpha = max(alpha, best_max_score);
                beta = min(beta, best_min_score);

                // if (alpha >= beta)
                //     break;
            }
        }
        return best_max_score;
    }

    uint16_t find_best_move(Player me, Player opponent, pair<uint64_t, uint64_t> food_board, int max_depth) {

        int best_score = INT32_MIN;
        uint16_t best_move;

        int depth = 0;
        auto start_time = chrono::high_resolution_clock::now();
        int nodes_visited = 0;

        const vector<uint16_t>& my_move_board = precompute_moves[me.body_arr[me.head_idx & ARR_SIZE]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.body_arr[opponent.head_idx & ARR_SIZE]];

        // while (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count() < 50 && depth < max_depth) {
        while (depth < max_depth) {
            for (const uint16_t& my_move : my_move_board) {
                pair<uint64_t, uint64_t> my_food_board = food_board;

                Player temp_me = me;
                temp_me.step_by_index(my_move);
                update_food(temp_me, my_food_board);

                for (const uint16_t& opponent_move : opponent_move_board) {
                    pair<uint64_t, uint64_t> opponent_food_board = food_board;

                    Player temp_opponent = opponent;
                    temp_opponent.step_by_index(opponent_move);
                    update_food(temp_opponent, opponent_food_board);
                    update_positions(temp_me, temp_opponent);
                    nodes_visited++;
                    pair<uint64_t, uint64_t> new_food;
                    new_food.first = my_food_board.first & opponent_food_board.first;
                    new_food.second = my_food_board.second & opponent_food_board.second;
                    int score = minimax(temp_me, temp_opponent, new_food, depth, INT32_MIN, INT32_MAX, nodes_visited);
                    
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
    void print_board(Player me, Player opponent, uint64_t food_board_firsthalf, uint64_t food_board_secondhalf) const {
        cout << "me length " << me.length << endl;
        cout << "opponent length " << opponent.length << endl;
        for (int i = BOARD_SIZE - 1; i >= 0; --i) {
            for (int j = BOARD_SIZE - 1; j >=0; --j) {
                int idx = i * BOARD_SIZE + j;
                if (idx < 64) {
                    uint64_t val = 1ULL << idx;
                    if (me.snake_head_board.first & val)
                        cout << "M ";
                    else if (me.snake_body_board.first & val)
                        cout << "1 ";
                    else if (opponent.snake_head_board.first & val)
                        cout << "O ";
                    else if (opponent.snake_body_board.first & val)
                        cout << "2 ";
                    else if (food_board_firsthalf & val)
                        cout << "F ";
                    else
                        cout << "| ";
                }
                else {
                    uint64_t val = 1ULL << (idx & 63);
                    if (me.snake_head_board.second & val)
                        cout << "M ";
                    else if (me.snake_body_board.second & val)
                        cout << "1 ";
                    else if (opponent.snake_head_board.second & val)
                        cout << "O ";
                    else if (opponent.snake_body_board.second & val)
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

    void print_player_board(Player player) const {
        cout << "player length " << player.length << endl;
        for (int i = BOARD_SIZE - 1; i >= 0; --i) {
            for (int j = BOARD_SIZE - 1; j >= 0; --j) {
                int idx = i * BOARD_SIZE + j;
                if (idx < 64) {
                    uint64_t val = 1ULL << idx;
                    if (player.snake_head_board.first & val)
                        cout << "H ";
                    else if (player.snake_body_board.first & val)
                        cout << "B ";
                    else
                        cout << "| ";
                }
                else {
                    uint64_t val = 1ULL << (idx & 63);
                    if (player.snake_head_board.second & val)
                        cout << "H ";
                    else if (player.snake_body_board.second & val)
                        cout << "B ";
                    else
                        cout << "| ";
                }
            }
            cout << endl;
        }
        cout << endl;
    }
};

void benchmark() {
    Game game;
    uint16_t my_starting_idx = 12;
    uint16_t opponent_starting_idx = 20; 
    
    uint16_t food[] = {8, 2, 60};

    pair<uint64_t, uint64_t> food_board = {0ULL, 0ULL};

    for (uint16_t f : food) {
        if (f < 64)
            food_board.first |= 1ULL << f;
        else
            food_board.second |= 1ULL << (f & 63);
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);
    // game.print_board(game.me, game.opponent, game.food_board_firsthalf, game.food_board_secondhalf);

    auto start_time = chrono::high_resolution_clock::now();
    uint16_t move = game.find_best_move(game.me, game.opponent, game.food_board, 8);

    auto end_time = chrono::high_resolution_clock::now();
    double time_taken = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
    cout << "total time: " << time_taken << endl;
}


int main() {

    precomp_moves();
    benchmark();

    return 0;
}