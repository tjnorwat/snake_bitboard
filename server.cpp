#include "crow.h"
#include <random>
#include <array>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include "rapidjson/document.h"
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using boost::multiprecision::uint128_t;

const uint32_t hash_size = 4294967295; // 2^32 - 1
const uint16_t arr_size = 127; // 2^7 - 1 


const uint128_t LEFT_COLUMN = uint128_t("1329877349959700883100633541501780992");
const uint128_t RIGHT_COLUMN = uint128_t("1298708349570020393652962442872833");
const uint128_t TOP_ROW = uint128_t("2657157917355198038900481496478384128");
const uint128_t BOTTOM_ROW = uint128_t("2047");


enum Direction {
    LEFT,
    RIGHT,
    UP,
    DOWN
};

array<array<vector<uint16_t>, 121>, 4> precompute_moves;

void precomp_moves() {
    for (uint16_t direction = LEFT; direction <= DOWN; direction++) {
        for (uint16_t i = 0; i <= 120; i++) {
            switch (direction) {
                case LEFT:
                    if (i >= 11) precompute_moves[direction][i].push_back(i - 11);
                    if (i < 120) precompute_moves[direction][i].push_back(i + 1);
                    if (i <= 120 - 11) precompute_moves[direction][i].push_back(i + 11);
                    break;
                case RIGHT:
                    if (i >= 11) precompute_moves[direction][i].push_back(i - 11);
                    if (i > 0) precompute_moves[direction][i].push_back(i - 1);
                    if (i <= 120 - 11) precompute_moves[direction][i].push_back(i + 11);
                    break;
                case UP:
                    if (i > 0) precompute_moves[direction][i].push_back(i - 1);
                    if (i < 120) precompute_moves[direction][i].push_back(i + 1);
                    if (i <= 120 - 11) precompute_moves[direction][i].push_back(i + 11);
                    break;
                default:
                    if (i >= 11) precompute_moves[direction][i].push_back(i - 11);
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
//     tt hash_table[hash_size];

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
//         for (int i = 0; i < hash_size; i++) {
//             hash_table[i].hash_key = 0ULL;
//             hash_table[i].flag = EXACT;
//             hash_table[i].depth = 0;
//             hash_table[i].value = 0;
//         }
//     }


//     void write_hash_entry(Hash_Flag flag, int depth, int value) {
//         tt *hash_entry = &hash_table[hash_key & hash_size];

//         hash_entry->hash_key = hash_key;
//     }


// };

// using size 11 board for offset 
array<Direction, 2*11 + 1> direction_lookup;
// Direction direction_lookup[2*11 + 1];

void init_direction_lookup() {
    direction_lookup[1 + 11] = LEFT;
    direction_lookup[-1 + 11] = RIGHT;
    direction_lookup[11 + 11] = UP;
    direction_lookup[-11 + 11] = DOWN;
}

struct Player {
    int16_t health;
    uint16_t length;
    Direction direction;
    uint128_t old_head_board;
    uint128_t snake_head_board;
    uint128_t snake_body_board;

    uint16_t body_arr[arr_size] = {0}; // using std::array is worse
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

        this->snake_head_board = uint128_t(1) << starting_idx;
        this->old_head_board = uint128_t(0);
        this->snake_body_board = uint128_t(0);
    }


    // implement direction 2darray instead of switch ? 

    void step_by_index(uint16_t idx) {
        uint128_t new_head = uint128_t(1) << idx;

        this->head_idx++;
        this->body_arr[this->head_idx & arr_size] = idx;

        this->snake_body_board |= this->snake_head_board;
        this->old_head_board = this->snake_head_board;
        this->snake_head_board = new_head;
        int dir = idx - this->body_arr[(this->head_idx - 1) & arr_size];
        this->direction = direction_lookup[dir + 11]; // unordered map is worse 

    }
};


struct Game {
    Player me;
    Player opponent;
    uint16_t size;
    int16_t total_turns;
    uint128_t food_board;

    Game() {
        this->size = 11;
        this->total_turns = 0;
        this->food_board = uint128_t(0);
    }

    // prints out me, opponent, and food boards
    void print_board(Player me, Player opponent, uint128_t food_board) const {
        cout << "me length " << me.length << endl;
        cout << "opponent length " << opponent.length << endl;
        for (int i = this->size - 1; i >= 0; --i) {
            for (int j = this->size - 1; j >=0; --j) {
                uint128_t idx = uint128_t(1) << (i * this->size + j);
                if (me.snake_head_board & idx)
                    cout << "M ";
                else if (me.snake_body_board & idx)
                    cout << "1 ";
                else if (opponent.snake_head_board & idx) 
                    cout << "O ";
                else if (opponent.snake_body_board & idx) 
                    cout << "2 ";
                else if (food_board & idx) 
                    cout << "F ";
                else 
                    cout << "| ";
            }
            cout << endl;
        }
        cout << endl;
    }

    void print_individual_board(uint128_t board) const {
        for (int i = this->size - 1; i >= 0; --i) {
            for (int j = this->size - 1; j >= 0; --j) {
                uint128_t idx = uint128_t(1) << (i * this->size + j);
                if (board & idx) 
                    cout << "1 ";
                else
                    cout << "| ";
            }
            cout << endl;
        }
        cout << endl;
    }

    // for the /start endpoint 
    void set_starting_position(uint16_t mysnake_idx, uint16_t opponent_idx, uint128_t food_board) {
        this->me = Player(mysnake_idx);
        this->opponent = Player(opponent_idx);
        this->food_board = food_board;
    }


    void update_food(Player &player, uint128_t &food_board) {
        if (player.snake_head_board & food_board) {
            food_board ^= player.snake_head_board;
            player.length++;
            if (!player.just_ate_apple) { 
                player.health = 100;
                player.snake_body_board ^= uint128_t(1) << player.body_arr[player.tail_idx & arr_size];
                player.tail_idx++;
            }
            player.just_ate_apple = true;
        }
        else {
            player.health--;
            if (!player.just_ate_apple) {
                player.snake_body_board ^= uint128_t(1) << player.body_arr[player.tail_idx & arr_size];
                player.tail_idx++;
            }
            player.just_ate_apple = false;
        }
    }




    // wont get the last couple msb for first 3 moves, but shouldnt matter 
    // need to add num_food_on_board to update_food()
    // need to figure out how to not use lsb function
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

   // make bitmasks const and global 
   void check_if_done(Player &player, const uint128_t all_boards) {
        if ((player.old_head_board & LEFT_COLUMN && player.direction == LEFT) ||
            (player.old_head_board & RIGHT_COLUMN && player.direction == RIGHT) ||
            (player.old_head_board & TOP_ROW && player.direction == UP) ||
            (player.old_head_board & BOTTOM_ROW && player.direction == DOWN) ||
            (player.snake_head_board & all_boards) ||
            (player.health <= 0)) {

            player.done = true;
        }
   }

    // update if we die, ate apple, and body positions 
    void update_positions(Player &me, Player &opponent) {
        const uint128_t all_boards = me.snake_body_board | opponent.snake_body_board;

        this->check_if_done(me, all_boards);
        this->check_if_done(opponent, all_boards);


        // need to check if either snake dies before checking if ran into head; 
        // ex. ran into wall wraps around to other side which could kill other snake
        if (me.done || opponent.done) {
            return;
        }

        // if either runs into others head, check which would win 
        if (me.snake_head_board & opponent.snake_head_board){
            if (me.length > opponent.length) {
                opponent.done = true;
            }
            else if (me.length < opponent.length) {
                me.done = true;
            }
            else {
                me.done = true;
                opponent.done = true;
            }
        }
    }

    int evaluate(Player &me, Player &opponent, int &depth) const {

        int score = 0;

        int my_score = 0;
        int opponent_score = 0;

        // my_score += me.length;
        // opponent_score += opponent.length; 

        // // my_score += head_score[me.body_arr[me.head_idx]];
        // // opponent_score += head_score[opponent.body_arr[opponent.head_idx]];


        // if (me.done) {
        //     my_score = -1000 + depth;
        // }
        
        // if (opponent.done) {
        //     opponent_score = -1000 + depth;
        // }

        // score += my_score;
        // score -= opponent_score;

        // return score;


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

    int minimax(Player &me, Player &opponent, uint128_t food_board, int depth, int alpha, int beta, int &nodes_visited) {

        if (depth == 0 || me.done || opponent.done){
            if (me.done && opponent.done) {
                return -1000 + depth;
            }
            if (me.just_ate_apple && opponent.just_ate_apple) {
                return 100 - depth;
            }
            return evaluate(me, opponent, depth);
        }

        int best_max_score = INT32_MIN;
        int best_min_score = INT32_MAX;

        const vector<uint16_t>& my_move_board = precompute_moves[me.direction][me.body_arr[me.head_idx & arr_size]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.direction][opponent.body_arr[opponent.head_idx & arr_size]];

        for (const uint16_t my_move : my_move_board) {
            uint128_t my_food_board = food_board;

            Player temp_me = me;
            temp_me.step_by_index(my_move);
            update_food(temp_me, my_food_board);

            for (const uint16_t opponent_move : opponent_move_board) {
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

    uint16_t find_best_move(Player me, Player opponent, uint128_t food_board, int max_depth) {

        int best_score = INT32_MIN;
        uint16_t best_move;

        int depth = 0;
        auto start_time = chrono::high_resolution_clock::now();
        int nodes_visited = 0;

        const vector<uint16_t>& my_move_board = precompute_moves[me.direction][me.body_arr[me.head_idx & arr_size]];
        const vector<uint16_t>& opponent_move_board = precompute_moves[opponent.direction][opponent.body_arr[opponent.head_idx & arr_size]];

        while (depth < max_depth) {
            for (const uint16_t my_move : my_move_board) {
                uint128_t my_food_board = food_board;

                Player temp_me = me;
                temp_me.step_by_index(my_move);
                update_food(temp_me, my_food_board);

                for (const uint16_t opponent_move : opponent_move_board) {
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
};


void testing() {
    Game game;

    uint16_t my_starting_idx = 61; // 12 // 61
    uint16_t opponent_starting_idx = 59; // 20
    
    uint16_t food[] = {8, 2, 60, 83, 81};
    uint128_t food_board = uint128_t(0);
    for (uint16_t f : food) {
        food_board |= uint128_t(1) << f;
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);
    game.print_board(game.me, game.opponent, game.food_board);

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
    uint128_t food_board = uint128_t(0);
    for (uint16_t f : food) {
        food_board |= uint128_t(1) << f;
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);
    // game.print_board(game.me, game.opponent, game.food_board);

    auto start_time = chrono::high_resolution_clock::now();
    uint16_t move = game.find_best_move(game.me, game.opponent, game.food_board, 15);

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
        uint128_t food_board = uint128_t(0);
        for (rapidjson::SizeType i = 0; i < food.Size(); i++) {
            food_board |= uint128_t(1) << (11 * food[i]["y"].GetInt() + (11 - food[i]["x"].GetInt()) - 1);
        }

        game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);

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
        uint128_t food_board = uint128_t(0);
        for (rapidjson::SizeType i = 0; i < food.Size(); i++) {
            food_board |= uint128_t(1) << (11 * food[i]["y"].GetInt() + 11 - food[i]["x"].GetInt()) - 1;
        }

        //should try and update opponent move and my previous move that i sent to the server 
        if (turn != 0){
            // exit(0);
            // has to be before updating food
            game.me.step_by_index(my_prev_move);
            game.opponent.step_by_index(opponent_idx);
            game.update_food(game.me, game.food_board);
            game.update_food(game.opponent, game.food_board);
            game.update_positions(game.me, game.opponent);

            game.food_board = food_board;
        }

        cout << "turn " <<  turn << endl;
        game.print_board(game.me, game.opponent, game.food_board);

        // getting the best move 
        uint16_t move_idx = game.find_best_move(game.me, game.opponent, game.food_board, 50);
        my_prev_move = move_idx; // will actually move the snake later with opponent 
        // get the direction which ai picked from the index 
        int dir = move_idx - boost::multiprecision::lsb(game.me.snake_head_board);
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
        uint128_t food_board = uint128_t(0);
        for (rapidjson::SizeType i = 0; i < food.Size(); i++) {
            food_board |= uint128_t(1) << (11 * food[i]["y"].GetInt() + 11 - food[i]["x"].GetInt()) - 1;
        }
        uint128_t temp_food = food_board;
        game.me.step_by_index(my_prev_move);
        game.opponent.step_by_index(opponent_idx);
        game.update_food(game.me, temp_food);
        temp_food = food_board;
        game.update_food(game.opponent, temp_food);
        game.update_positions(game.me, game.opponent);

        game.food_board = food_board;

        game.print_board(game.me, game.opponent, game.food_board);
        cout << "ENDED" << endl;
        // game = Game(); // reset game 
        return "";
    });

    // app.port(8080).run();

    app.port(8080).multithreaded().run();

}