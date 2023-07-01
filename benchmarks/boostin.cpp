#include <iostream>
#include <cstdint>
#include <chrono>
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using boost::multiprecision::uint128_t;


void boostin(int times) {
    uint128_t x = uint128_t(1) << 127;
    for (int i = 0; i < times; i++){
        x ^= uint128_t(1) << (i % 126);
        uint16_t idx = boost::multiprecision::lsb(x);
    }
}


void no_boost(int times) {
    __uint128_t x = __uint128_t(1) << 127;
    for (int i = 0; i < times; i++) {
        x ^= __uint128_t(1) << (i % 126);
        uint16_t idx = __builtin_ctz(x);
    }
}


int main() {

    int times = 1000000000;

    chrono::time_point<chrono::steady_clock> start1, end1, start2, end2;
    double elapsed_time1, elapsed_time2;



    // start1 = chrono::steady_clock::now();
    // boostin(times);
    // end1 = chrono::steady_clock::now();
    // elapsed_time1 = double(chrono::duration_cast <chrono::nanoseconds> (end1 - start1).count());

    // cout << "nanoseconds: " << elapsed_time1 << endl;

    start2 = chrono::steady_clock::now();
    no_boost(times);
    end2 = chrono::steady_clock::now();
    elapsed_time2 = double(chrono::duration_cast <chrono::nanoseconds> (end2 - start2).count());

    cout << "nanoseconds: " << elapsed_time2 << endl;



    return 0;
}