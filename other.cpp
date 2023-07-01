#include "crow.h"
#include <deque>
#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include "rapidjson/document.h"
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using boost::multiprecision::uint128_t;


enum Direction {
    LEFT,
    RIGHT,
    UP,
    DOWN
};

struct Player {
    int16_t health;
    uint16_t length;
    Direction direction;
    deque<uint16_t> body_list;
    uint128_t old_head_board;
    uint128_t snake_head_board;
    uint128_t snake_body_board;
    bool done;
    bool just_ate_apple;


    Player() {}

    Player(uint16_t starting_idx) {
        this->health = 100;
        this->length = 3;
        this->direction = UP;
        this->done = false;
        this->just_ate_apple = false;
        this->body_list = deque<uint16_t>(1, starting_idx);
        this->snake_head_board = uint128_t(1) << starting_idx;
        this->old_head_board = uint128_t(0);
        this->snake_body_board = uint128_t(0);
    }

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
};


struct Game {
    Player me;
    Player opponent;
    uint16_t size;
    int16_t total_turns;
    uint128_t food_board;
    uint128_t first_row;
    uint128_t last_row;
    uint128_t last_column;
    uint128_t first_column;
    vector<uint32_t> food_choices;


    Game() {}

    Game(uint16_t size) : size(size) {

        this->total_turns = 0;

        this->first_column = uint128_t(0);
        this->last_column = uint128_t(0);
        this->first_row = uint128_t(0);
        this->last_row = uint128_t(0);

        for (uint16_t i = 0; i < size; ++i) {
            this->first_column |= uint128_t(1) << (size * i + size - 1);
            this->last_column |= uint128_t(1) << (size * i);
            this->first_row |= uint128_t(1) << (size * size - i - 1);
            this->last_row |= uint128_t(1) << i;
        }

        this->food_board = uint128_t(0);
    }

    // prints out me, opponent, and food boards
    void print_board(Player me, Player opponent, uint128_t food_board) const {
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

    // helper function for update_positions
    // updates player attributes if ate apple
    // TODO : update total_turns 
    void update_food(Player &player, uint128_t &food_board, uint16_t turns) {
        if (player.snake_head_board & food_board) {
            player.health = 100;
            player.length++;
            food_board ^= player.snake_head_board; // removing food 
            if (!player.just_ate_apple) {
                
                if(this->total_turns > turns){
                    uint16_t idx_to_remove = player.body_list.back();
                    player.body_list.pop_back();
                    player.snake_body_board ^= uint128_t(1) << idx_to_remove;
                }
            }
            player.just_ate_apple = true;
        }
        else {
            player.health--;
            // if we just ate apple, dont pop back 
            if (player.just_ate_apple) {
                player.just_ate_apple = false;
            }
            else {
                // if this is the third or more turn, we can remove from body board 
                if (this->total_turns > turns) {
                    uint16_t idx_to_remove = player.body_list.back();
                    player.body_list.pop_back();
                    player.snake_body_board ^= uint128_t(1) << idx_to_remove;
                }
            }
        }
    }


    // update condition doesnt take into account the top left corner 
    // are there other corners like that?
    // update if we die, ate apple, and body positions 
    void update_positions(Player &me, Player &opponent) {
        
        // important to note that if any of the snakes die, we return instantly

        // see if we die first 
        // do this first because if both players eat apple, then we remove food and other player would have skewed length
        
        // check if player hit wall first 
        // check wall first because if we also run into opponents body when they hit wall, we dont die 
        if (me.old_head_board & this->first_column && me.snake_head_board & this->last_column){
            me.done = true;
            return;
        }
        else if (me.old_head_board & this->last_column && me.snake_head_board & this->first_column) {
            me.done = true;
            return;
        }
        else if (me.old_head_board & this->first_row && me.direction == UP) {
            me.done = true;
            return;
        }
        else if (me.old_head_board & this->last_row && me.direction == DOWN) {
            me.done = true;
            return;
        }

        // opponent runs into wall 
        if (opponent.old_head_board & this->first_column && opponent.snake_head_board & this->last_column) {
            opponent.done = true;
            return;
        }
        else if (opponent.old_head_board & this->last_column && opponent.snake_head_board & this->first_column) {
            opponent.done = true;
            return;
        }
        else if (opponent.old_head_board & this->first_row && opponent.direction == UP) {
            opponent.done = true;
            return;
        }
        else if (opponent.old_head_board & this->last_row && opponent.direction == DOWN) {
            opponent.done = true;
            return;
        }


        // if either runs into others head, check which would win 
        if (me.snake_head_board & opponent.snake_head_board){
            if (me.length > opponent.length) {
                opponent.done = true;
                return;
            }
            else if (me.length < opponent.length) {
                me.done = true;
                return;
            }
            me.done = true;
            opponent.done = true;
            return;
        }

        uint128_t all_boards = me.snake_body_board | opponent.snake_body_board;
        // if either snake runs into itself / opponent

        if (me.snake_head_board & all_boards && opponent.snake_body_board & all_boards) {
            me.done = true;
            opponent.done = true;
            return;
        }
        else if (me.snake_head_board & all_boards) {
            me.done = true;
            return;
        }
        else if (opponent.snake_head_board & all_boards) {
            opponent.done = true;
            return;
        }

        // have to check health after we check if ate food 
        if (me.health <= 0 && opponent.health <= 0){
            me.done = true;
            opponent.done = true;
            return;
        }
        else if (me.health <= 0) {
            me.done = true;
            return;
        }
        else if (opponent.health <= 0) {
            opponent.done = true;
            return;
        } 
        
        //does this fix the issue? 
        me.done = false;
        opponent.done = false; // getting new opponent move every time 
    }

    int evaluate(Player player, int depth) {

        if (player.done)
            return -1000 + depth;
        else if (player.just_ate_apple)
            return 100 - depth;

        return 0;
    }

    pair<int, int> minimax(Player me, Player opponent, uint128_t food_board, int depth, int alpha, int beta, int &nodes_visited) {

        if (depth == 0 || me.done || opponent.done){
            return pair<int, int>(evaluate(me, depth), -evaluate(opponent, depth));
        }

        uint128_t my_move_board = possible_moves(me);
        uint128_t opponent_move_board = possible_moves(opponent);

        int best_max_score = INT32_MIN;
        int best_min_score = INT32_MAX;
        while(my_move_board) {
            uint128_t my_food_board = food_board;
            uint16_t my_move = boost::multiprecision::lsb(my_move_board);
            my_move_board ^= uint128_t(1) << my_move;

            Player temp_me = me;
            temp_me.step_by_index(my_move);
            update_food(temp_me, my_food_board, 0);

            uint128_t temp_opponent_move_board = opponent_move_board;
            while(temp_opponent_move_board) {
                uint128_t opponent_food_board = food_board;

                uint16_t opponent_move = boost::multiprecision::lsb(temp_opponent_move_board);
                temp_opponent_move_board ^= uint128_t(1) << opponent_move;
                
                Player temp_opponent = opponent;
                temp_opponent.step_by_index(opponent_move);
                
                update_food(temp_opponent, opponent_food_board, 0);
                update_positions(temp_me, temp_opponent);
                nodes_visited++;
                pair<int, int> score = minimax(temp_me, temp_opponent, my_food_board & opponent_food_board, depth - 1, alpha, beta, nodes_visited);

                best_min_score = min(best_min_score, score.second);
                best_max_score = max(best_max_score, best_min_score);
                alpha = max(alpha, best_max_score);

                beta = min(beta, best_min_score);

                if (alpha >= beta)
                    break;
            }
        }
        return pair<int, int> (best_max_score, best_min_score);   
    }

    uint16_t find_best_move(Player me, Player opponent, uint128_t food_board, int max_depth) {

        int best_val = INT32_MIN;
        uint16_t best_move;

        uint128_t my_move_board = possible_moves(me);
        uint128_t opponent_move_board = possible_moves(opponent);

        int depth = 0;
        int nodes_visited = 0;
        auto start_time = chrono::high_resolution_clock::now();
        // while (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count() < 50 && depth < max_depth) {
        // for (int i = 0; i < 1; i++) {
        while (depth < max_depth) {
            uint128_t temp_my_move_board = my_move_board;
            while(temp_my_move_board) {
                uint128_t my_food_board = food_board;
                uint16_t my_move = boost::multiprecision::lsb(temp_my_move_board);
                temp_my_move_board ^= uint128_t(1) << my_move;
                
                Player temp_me = me;
                temp_me.step_by_index(my_move);
                update_food(temp_me, my_food_board, 0);

                uint128_t temp_opponent_move_board = opponent_move_board;
                while(temp_opponent_move_board) {
                    uint128_t opponent_food_board = food_board;

                    uint16_t opponent_move = boost::multiprecision::lsb(temp_opponent_move_board);
                    temp_opponent_move_board ^= uint128_t(1) << opponent_move;

                    Player temp_opponent = opponent;
                    temp_opponent.step_by_index(opponent_move);

                    // updating body list before seeing if dead 
                    update_food(temp_opponent, opponent_food_board, 0);
                    update_positions(temp_me, temp_opponent);
                    nodes_visited++;
                    pair<int, int> score = minimax(temp_me, temp_opponent, my_food_board & opponent_food_board, depth, INT32_MIN, INT32_MAX, nodes_visited);

                    if (score.first > best_val){
                        best_val = score.first;
                        best_move = my_move;
                        // cout << "BEST MOVE: " << best_move << endl;
                    }

                    // cout << "my score " << score.first << endl;
                    // cout << "opponent score " << score.second << endl;
                    // print_board(temp_me, temp_opponent, my_food_board & opponent_food_board);
                }
            }
            ++depth;
        }
        cout << "depth " << depth << endl;
        cout << "nodes visited " << nodes_visited << endl;
        // cout << "best move : " << best_move << endl;
        return best_move;
    }

    uint128_t possible_moves(Player player) const {

        uint16_t head_idx = boost::multiprecision::lsb(player.snake_head_board);
        uint128_t move_board = uint128_t(0);

        switch (player.direction) {
            case LEFT:
                move_board |= uint128_t(1) << (head_idx + 1);
                move_board |= uint128_t(1) << (head_idx + this->size);
                move_board |= uint128_t(1) << (head_idx - this->size);
                break;
            case RIGHT:
                move_board |= uint128_t(1) << (head_idx - 1);
                move_board |= uint128_t(1) << (head_idx + this->size);
                move_board |= uint128_t(1) << (head_idx - this->size);
                break;
            case UP:
                move_board |= uint128_t(1) << (head_idx + 1);
                move_board |= uint128_t(1) << (head_idx - 1);
                move_board |= uint128_t(1) << (head_idx + this->size);
                break;
            default:
                move_board |= uint128_t(1) << (head_idx + 1);
                move_board |= uint128_t(1) << (head_idx - 1);
                move_board |= uint128_t(1) << (head_idx - this->size);
                break;
        }

        // move_board |= uint128_t(1) << (head_idx + 1);
        // move_board |= uint128_t(1) << (head_idx - 1);
        // move_board |= uint128_t(1) << (head_idx + this->size);
        // move_board |= uint128_t(1) << (head_idx - this->size);

        return move_board;
    }
};


void testing() {
    Game game(11);

    uint16_t my_starting_idx = 120; // 12 // 61
    uint16_t opponent_starting_idx = 59; // 20
    
    uint16_t food[] = {8, 2, 60};
    uint128_t food_board = uint128_t(0);
    for (uint16_t f : food) {
        food_board |= uint128_t(1) << f;
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);
    game.print_board(game.me, game.opponent, game.food_board);
    game.find_best_move(game.me, game.opponent, game.food_board, 1);
}

void benchmark() {
    Game game(11);

    uint16_t my_starting_idx = 12;
    uint16_t opponent_starting_idx = 20; 
    
    uint16_t food[] = {8, 2, 60};
    uint128_t food_board = uint128_t(0);
    for (uint16_t f : food) {
        food_board |= uint128_t(1) << f;
    }

    game.set_starting_position(my_starting_idx, opponent_starting_idx, food_board);
    game.print_board(game.me, game.opponent, game.food_board);

    auto start_time = chrono::high_resolution_clock::now();
    game.find_best_move(game.me, game.opponent, game.food_board, 15);

    auto end_time = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count();
    cout << "total time: " << end_time << endl;
}


int main()
{

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
        game = Game(11);
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
            game.update_food(game.me, game.food_board, 2);
            game.update_food(game.opponent, game.food_board, 2);
            game.update_positions(game.me, game.opponent);

            game.food_board = food_board;
        }


        // cout << "my body ";
        // for(int i = 0; i < game.me.body_list.size(); i++) {
        //     cout << game.me.body_list[i] << " ";
        // }
        // cout << endl;
        // cout << "Turn " << turn << endl;
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
        game.update_food(game.me, temp_food, 2);
        temp_food = food_board;
        game.update_food(game.opponent, temp_food, 2);
        game.update_positions(game.me, game.opponent);

        game.food_board = food_board;

        game.print_board(game.me, game.opponent, game.food_board);
        cout << "ENDED" << endl;
        game = Game(11); // reset game 
        return "";
    });

    app.port(8080).multithreaded().run();
}