#include <deque>
#include <random>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>

using boost::multiprecision::uint128_t;
using namespace std;


struct Player {
    uint16_t length;
    uint16_t health;
    uint16_t direction;
    std::deque<uint16_t> body_list;
    uint128_t snake_head_board;
    uint128_t old_head_board;
    uint128_t snake_body_board;
    bool done;
    bool just_ate_apple;

    Player();
    Player(uint16_t starting_idx);
    void step(uint16_t action);
    void step_by_index(uint16_t idx);
};

Player::Player() {}

Player::Player(uint16_t starting_idx) {
    this->length = 3;
    this->health = 100;
    this->direction = 2;
    this->done = false;
    this->just_ate_apple = false;
    this->body_list = deque<uint16_t>(3, starting_idx);
    this->snake_head_board = uint128_t(1) << starting_idx;
    this->old_head_board = uint128_t(0);
    this->snake_body_board = uint128_t(0);
}

// update direction? 
void Player::step_by_index(uint16_t idx) {
    uint128_t new_head = uint128_t(1) << idx;
    body_list.push_front(idx);
    this->old_head_board = this->snake_head_board;
    this->snake_head_board = new_head;
    this->health--;
    int dir = idx - boost::multiprecision::lsb(this->old_head_board);
    snake_body_board |= new_head;
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


// game will calculate players death 
void Player::step(uint16_t action) {
    uint128_t new_head = this->snake_head_board;

    // left, right, up, down
    switch (action) {
        case 0:
            new_head <<= 1;
            this->direction = 0;
            break;
        case 1:
            new_head >>= 1;
            this->direction = 1;
            break;
        case 2:
            new_head <<= 11; // size of the board 
            this->direction = 2;
            break;
        default:
            new_head >>= 11;
            this->direction = 3;
            break;
    }
    
    uint16_t idx = boost::multiprecision::lsb(new_head);
    body_list.push_front(idx);

    snake_body_board |= snake_head_board;
    // do i need this? 
    this->old_head_board = snake_head_board;
    snake_head_board = new_head;
    // game updates if we died or not 
}