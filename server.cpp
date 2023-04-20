#include "crow.h"
#include <deque>
#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include "rapidjson/document.h"
#include <boost/multiprecision/cpp_int.hpp>


// constexpr uint128_t OUT_OF_BOUNDS = 0xFE000000000000000000000000000000;
// constexpr uint128_t FIRST_COLUMN = 0x1002004008010020040080100200400;
// constexpr uint128_t LAST_COLUMN = 0x4008010020040080100200400801;
// constexpr uint128_t FIRST_ROW = 0x1FFC000000000000000000000000000;

using boost::multiprecision::uint128_t;
using namespace std;

struct Player {
    uint16_t health;
    uint16_t direction;
    deque<uint16_t> body_list;
    uint128_t snake_head_board;
    uint128_t old_head_board;
    uint128_t snake_body_board;
    bool done;
    bool just_ate_apple;

    Player();
    Player(uint16_t starting_idx);
    void step_by_index(uint16_t idx);
};

Player::Player() {}

Player::Player(uint16_t starting_idx) {
    this->health = 100;
    this->direction = 2;
    this->done = false;
    this->just_ate_apple = false;
    this->body_list = deque<uint16_t>(1, starting_idx);
    this->snake_head_board = uint128_t(1) << starting_idx;
    this->old_head_board = uint128_t(0);
    this->snake_body_board = uint128_t(0);
}

void Player::step_by_index(uint16_t idx) {
    uint128_t new_head = uint128_t(1) << idx;
    this->body_list.push_front(idx);
    this->snake_body_board |= this->snake_head_board;
    this->old_head_board = this->snake_head_board;
    this->snake_head_board = new_head;
    this->health--;
    int dir = idx - boost::multiprecision::lsb(this->old_head_board);
    switch (dir)
    {
    case 1:
        this->direction = 0;
        break;
    case -1:
        this->direction = 1;
        break;
    case 11:
        this->direction = 2;
        break;
    default:
        this->direction = 3;
        break;
    }
}

struct Game {
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
    void print_board(Player me, Player opponent, uint128_t food_board) const;
    void print_individual_board(uint128_t board) const;
    void set_starting_position(uint16_t snake1_idx, uint16_t snake2_idx, uint128_t food_board); // /start endpoint 
    void update_position(Player &player, uint128_t &food_board, uint16_t turns); // will take care of /move endpoint 
    int minimax(Player me, Player opponent, uint128_t food_board, uint16_t depth, int alpha, int beta, bool maximizing_player);
    uint16_t find_best_move(Player me, Player opponent, uint128_t food_board);
    void update_opponent_move(int move);
    int evaluate(Player me, Player opponent, uint128_t food_board);
    uint128_t possible_moves(uint128_t head_board);
};

Game::Game() {}

Game::Game(uint16_t size) : size(size) {

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

    this->out_of_bounds = uint128_t(0);
    for (int i = 121; i < 128; ++i) {
        this->out_of_bounds |= uint128_t(1) << i;
    }

    this->food_board = uint128_t(0);

}


void Game::set_starting_position(uint16_t mysnake_idx, uint16_t opponent_idx, uint128_t food_board) {
    this->me = Player(mysnake_idx);
    this->opponent = Player(opponent_idx);
    this->food_board = food_board;
}

// updates the move from opponent after first turn 
void Game::update_opponent_move(int new_head_idx) {
    this->opponent.step_by_index(new_head_idx);
    update_position(this->opponent, this->food_board, 3); // see if opponent ate apple 
}

// updates player attributes if ate apple
void Game::update_position(Player &player, uint128_t &food_board, uint16_t turns) {
    if (player.snake_head_board & food_board) {
        player.health = 100;
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


int Game::evaluate(Player me, Player opponent, uint128_t food_board) {

    // check if player hit wall first 
    // check wall first because if we also run into opponents body when they hit wall, we dont die 
    
    if (me.old_head_board & this->first_column && me.snake_head_board & this->last_column)
        return -1000;
    else if (me.old_head_board & this->last_column && me.snake_head_board & this->first_column)
        return -1000;
    else if (me.old_head_board & this->first_row && me.direction == 2)
        return -1000;

    // opponent runs into wall 
    if (opponent.old_head_board & this->first_column && opponent.snake_head_board & this->last_column)
        return 1000;
    else if (opponent.old_head_board & this->last_column && opponent.snake_head_board & this->first_column)
        return 1000;
    else if (opponent.old_head_board & this->first_row && opponent.direction == 2)
        return 1000;


    // if either runs into others head, check which would win 
    if (me.snake_head_board & opponent.snake_head_board){
        if (me.body_list.size() > opponent.body_list.size()) {
            return 1000;
        }
        else if (me.body_list.size() < opponent.body_list.size()) {
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
    if (me.health <= 0) {
        return -1000;
    }
    else if (opponent.health <= 0) {
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


    // these are terminal states 
    if (score == 1000) {
        return score + depth;
    }
    else if (score == -1000) {
        return score - depth;
    }

    // terminal state ; if depth is 0 or the game has ended; can check if game has ended by the score (player dies) 
    if (depth == 0) {
        // std::cout << "length " << me.body_list.size() << std::endl;
        return score;
    }

    // im assuming this is going to act like an update function
    // which includes: removing food ; adjusting health ; removing body from boards
    
    // me 
    if (score == 10) {
        me.health = 100;
        if (!me.just_ate_apple) {
            uint16_t idx_to_remove = me.body_list.back();
            me.snake_body_board ^= uint128_t(1) << idx_to_remove;
            me.body_list.pop_back();
        }
        me.just_ate_apple = true;
        food_board ^= me.snake_head_board;
    }
    else {
        uint16_t idx_to_remove = me.body_list.back();
        me.snake_body_board ^= uint128_t(1) << idx_to_remove;
        me.body_list.pop_back();
        me.health--;
        me.just_ate_apple = false;
    }

    // opponent 
    if (score == -10) {
        opponent.health = 100;
        food_board ^= opponent.snake_head_board;
        if(!opponent.just_ate_apple) {
            uint16_t idx_to_remove = opponent.body_list.back();
            opponent.snake_body_board ^= uint128_t(1) << idx_to_remove;
            opponent.body_list.pop_back();
        }
        opponent.just_ate_apple = true;
    }
    else {
        uint16_t idx_to_remove = opponent.body_list.back();
        opponent.snake_body_board ^= uint128_t(1) << idx_to_remove;
        opponent.body_list.pop_back();
        opponent.health--;
        opponent.just_ate_apple = false;
    }

    if (maximizing_player) {
        int max_eval = INT32_MIN;
        uint128_t my_move_board = possible_moves(this->me.snake_head_board);
        uint128_t opponent_move_board = possible_moves(this->opponent.snake_head_board);

        while(my_move_board) {
            uint16_t my_move = boost::multiprecision::lsb(my_move_board);
            Player temp_me = me;
            me.step_by_index(my_move);
            uint128_t temp_opponent_move_board = opponent_move_board;
            while(temp_opponent_move_board) {

                uint16_t opponent_move = boost::multiprecision::lsb(temp_opponent_move_board);
                temp_opponent_move_board ^= opponent_move;
                
                // can we keep an old reference to 'undo' the move?
                Player temp_opponent = opponent;
                opponent.step_by_index(opponent_move);

                int move_val = minimax(me, opponent, food_board, depth - 1, alpha, beta, false);

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
            uint128_t temp_opponent_move_board = opponent_move_board;
            while(temp_opponent_move_board) {

                uint16_t opponent_move = boost::multiprecision::lsb(temp_opponent_move_board);
                temp_opponent_move_board ^= uint128_t(1) << opponent_move;
                
                // can we keep an old reference to 'undo' the move?
                Player temp_opponent = opponent;
                opponent.step_by_index(opponent_move);

                int move_val = minimax(me, opponent, food_board, depth - 1, alpha, beta, true);

                min_eval = min(min_eval, move_val);
                beta = min(beta, move_val);
                
                if (beta <= alpha) {
                    break;
                }

                // undo opponent;
                opponent = temp_opponent;
            }
            my_move_board ^= uint128_t(1) << my_move;
            // undo my move
            me = temp_me;
        }
        return min_eval;
    }
}

uint16_t Game::find_best_move(Player me, Player opponent, uint128_t food_board) {

    int best_val = INT32_MIN;
    uint16_t best_move;

    uint128_t my_move_board = possible_moves(this->me.snake_head_board);
    uint128_t opponent_move_board = possible_moves(this->opponent.snake_head_board);

    int depth = 0;
    auto start_time = chrono::high_resolution_clock::now();

    // will have to figure out how to get possible moves for players 
    while (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start_time).count() < 20) {
    // for (int i = 0; i < 1; i++) {
        // cout << "DEPTH " << i << endl;
        // std::cout << "TIME " << (chrono::high_resolution_clock::now() - start_time).count() << std::endl;
        // print_individual_board(my_move_board);
        while(my_move_board) {

            uint16_t my_move = boost::multiprecision::lsb(my_move_board);
            // cout << "my move " << my_move << endl;
            Player temp_me = me;
            me.step_by_index(my_move);
            uint128_t temp_opponent_move_board = opponent_move_board;
            while(temp_opponent_move_board) {

                uint16_t opponent_move = boost::multiprecision::lsb(temp_opponent_move_board);
                temp_opponent_move_board ^= uint128_t(1) << opponent_move;
                
                // can we keep an old reference to 'undo' the move?
                Player temp_opponent = opponent;
                opponent.step_by_index(opponent_move);

                int move_val = minimax(me, opponent, food_board, depth, INT32_MIN, INT32_MAX, false);

                if (move_val > best_val){
                    best_val = move_val;
                    best_move = my_move;
                }

                // print_board(me, opponent, food_board);
                // undo opponent;
                opponent = temp_opponent;
            }
            my_move_board ^= uint128_t(1) << my_move;
            // undo my move
            me = temp_me;
        }
        ++depth;
    }
    // std::cout << "depth " << depth << std::endl;
    this->me.step_by_index(best_move);

    // need to update if we ate apple 
    // not sure how to fix the removing of initial indexes. 
    // we are updating my snake before opponent or something.  
    update_position(this->me, this->food_board, 1);

    // i think the idea is to have the best move be the board position of the head and then calculate action/direction
    return this->me.direction;
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

void Game::print_individual_board(uint128_t board) const {
    for (int i = this->size - 1; i >= 0; --i) {
        for (int j = this->size - 1; j >= 0; --j) {
            uint128_t idx = uint128_t(1) << (i * this->size + j);
            if (board & idx) 
                std::cout << "1 ";
            else
                std::cout << "| ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}


// prints out me, opponent, and food boards
void Game::print_board(Player me, Player opponent, uint128_t food_board) const {
    for (int i = this->size - 1; i >= 0; --i) {
        for (int j = this->size - 1; j >=0; --j) {
            uint128_t idx = uint128_t(1) << (i * this->size + j);
            if (me.snake_head_board & idx)
                std::cout << "M ";
            else if (me.snake_body_board & idx)
                std::cout << "1 ";
            else if (opponent.snake_head_board & idx) 
                std::cout << "O ";
            else if (opponent.snake_body_board & idx) 
                std::cout << "2 ";
            else if (food_board & idx) 
                std::cout << "F ";
            else 
                std::cout << "| ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}


int main()
{
    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

    Game game;
    rapidjson::Document doc;


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

        int16_t my_starting_idx, opponent_starting_idx;
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
        
        // game.print_board(game.me, game.opponent, food_board);

        std::cout << "Starting" << std::endl;
        return "";
    });

    

    CROW_ROUTE(app, "/move")
    .methods("POST"_method)
    ([&](const crow::request& req){
        doc.Parse(req.body.c_str());

        uint16_t turn = doc["turn"].GetInt();
        // std::cout << "turn " << turn << std::endl;
        
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

        if (turn != 0){
            game.update_opponent_move(opponent_idx); // has to be before updating food
            game.food_board = food_board;
        }

        // std::cout << "my body ";
        // for(int i = 0; i < game.me.body_list.size(); i++) {
        //     std::cout << game.me.body_list[i] << " ";
        // }
        // std::cout << std::endl;
        // game.print_board(game.me, game.opponent, food_board);

        // getting the best move 
        uint16_t move = game.find_best_move(game.me, game.opponent, food_board);

        crow::json::wvalue data;
        // std::cout << "Move is " << move << std::endl;
        if (move == 0)
            data["move"] = "left";
        else if (move == 1)
            data["move"] = "right";
        else if (move == 2) 
            data["move"] = "up";
        else
            data["move"] = "down";

        return data;
    });





    CROW_ROUTE(app, "/end")
    .methods("POST"_method)
    ([&](const crow::request& req){
        std::cout << "ENDED" << std::endl;        
        return "";
    });

    app.port(8080).multithreaded().run();
}