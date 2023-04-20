#include <boost/multiprecision/cpp_int.hpp>
#include <deque>
#include <iostream>
#include <random>
#include <vector>
#include <chrono>

using boost::multiprecision::uint128_t;

struct Game {
    uint16_t size;
    bool done;
    uint16_t score;
    int8_t health;
    uint16_t starting_idx;
    std::deque<uint16_t> body_list;
    uint128_t snake_head_board;
    uint128_t snake_body_board;
    uint128_t food_board;
    uint128_t first_column;
    uint128_t last_column;
    uint128_t first_row;
    uint128_t out_of_bounds;
    std::vector<uint32_t> food_choices;

    Game(uint16_t size);
    void step(uint16_t action);
    void get_food();
    void reset();
    void print_board() const;
    void print_individual_board(uint128_t board) const;
};

Game::Game(uint16_t size) : size(size), done(false), score(0), health(100) {
    starting_idx = static_cast<uint16_t>(static_cast<float>(size * size) / 2.0f);

    snake_head_board = uint128_t(1) << starting_idx;

    first_column = 0;
    last_column = 0;
    first_row = 0;

    for (uint16_t i = 0; i < size; ++i) {
        first_column |= uint128_t(1) << (size * i + size - 1);
        last_column |= uint128_t(1) << (size * i);
        first_row |= uint128_t(1) << (size * size - i - 1);
    }

    out_of_bounds = 0;
    for (int i = 121; i < 128; ++i) {
        out_of_bounds |= uint128_t(1) << i;
    }

    body_list = std::deque<uint16_t>({starting_idx});

    snake_body_board = uint128_t(0);
    food_board = uint128_t(0);

    get_food();
}

void Game::step(uint16_t action) {
    uint128_t new_head = snake_head_board;

    // left, right, up, down
    switch (action) {
        case 0:
            new_head <<= 1;
            break;
        case 1:
            new_head >>= 1;
            break;
        case 2:
            new_head <<= size;
            break;
        default:
            new_head >>= size;
            break;
    }

    // moved upwards out of bounds 
    // have to put this first because new_head cannot be 0 when trying to find lsb
    if (new_head == 0) {
        done = true;
        return;
    }

    uint16_t idx = (boost::multiprecision::lsb(new_head));
    // std::cout << "index of new head" << idx << std::endl;
    body_list.push_front(idx);

    snake_body_board |= snake_head_board;

    uint128_t old_head = snake_head_board;
    snake_head_board = new_head;

    if (snake_head_board & food_board) {
        get_food();
        health = 100;
        score++;
    } else {
        health--;
        uint16_t idx_to_remove = body_list.back();
        body_list.pop_back();
        snake_body_board ^= uint128_t(1) << idx_to_remove;
    }

    if (new_head & snake_body_board) {
        done = true;
    } else if (old_head & first_column && action == 0) {
        done = true;
    } else if (old_head & last_column && action == 1) {
        done = true;
    } else if (old_head & first_row && action == 2) {
        done = true;
    } else if (health <= 0) {
    done = true;
    }
    
    
    // done = (new_head & snake_body_board) | (old_head & first_column & (action == 0)) | (old_head & last_column & (action == 1)) | (old_head & first_row & (action == 2)) | (health <= 0);

    // uint128_t check_done = (new_head & snake_body_board) | (old_head & first_column & (action == 0)) | (old_head & last_column & (action == 1)) | (old_head & first_row & (action == 2));
    // done = (check_done != 0) || (health <= 0);


}

void Game::get_food() {
    uint128_t all_boards = (~(snake_body_board | snake_head_board)) ^ out_of_bounds;
    
    food_choices.clear();
    // if all the spots are taken up, we can't put any food down 
    if (!all_boards) {
        food_board = uint128_t(0);
    }
    else {
        while (all_boards) {
        uint32_t index = boost::multiprecision::lsb(all_boards);
        food_choices.push_back(index);
        all_boards ^= uint128_t(1) << index;
        }

        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<size_t> dist(0, food_choices.size() - 1);
        size_t random_index = dist(rng);

        food_board = uint128_t(1) << food_choices[random_index];
    }
}

void Game::reset() {
    snake_head_board = uint128_t(1) << starting_idx;
    get_food();
    score = 0;
    done = false;
    health = 100;
    snake_body_board = uint128_t(0);
    body_list.clear();
    body_list.push_back(starting_idx);
}

void Game::print_board() const {
    for (int i = size - 1; i >= 0; --i) {
        for (int j = size - 1; j >= 0; --j) {
            uint128_t idx = uint128_t(1) << (i * size + j);
            if (snake_head_board & idx) {
                std::cout << "H ";
            } else if (food_board & idx) {
                std::cout << "F ";
            } else if (snake_body_board & idx) {
                std::cout << "B ";
            } else {
                std::cout << "| ";
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void Game::print_individual_board(uint128_t board) const {
    for (int i = size - 1; i >= 0; --i) {
        for (int j = size - 1; j >= 0; --j) {
            uint128_t idx = uint128_t(1) << (i * size + j);
            if (board & idx) {
                std::cout << "1 ";
            } else {
                std::cout << "0 ";
            }
        }
        std::cout << std::endl;
    }
}

void play_game() {
    Game game(11);
    game.print_board();
    uint16_t action;
    while (true) {
        std::cin >> action;

        game.step(action);
        game.print_board();

        if (game.done) {
            std::cout << "DEAD" << std::endl;

            game.reset();
            game.print_board();
        }
    }
}

void run_benchmark() {
    Game game(11);
    const int num_games = 100000;
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 3);
    uint16_t action;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_games; ++i) {
        game.reset();
        while (!game.done) {
            action = dist(rng);
            game.step(action);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);

    std::cout << "elapsed time " << elapsed_time.count() << " seconds" << std::endl;
}


void test() {
    Game game(11);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 3);
    uint16_t action = 2;

    int count = 0;
    while(!game.done) {
        game.step(action);
        count++;
    }
    std::cout << count << std::endl;

}

int main() {
    run_benchmark();
    return 0;
}