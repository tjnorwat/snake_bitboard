#include <deque>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include "veque.hpp"
#include "devector.hpp"
#include <boost/circular_buffer.hpp>

using namespace std;

// this dont make no sense 
void circular(uint16_t times) {
    boost::circular_buffer<uint16_t> body_list(7);

    for (uint16_t i = 1; i < 5; i++) {
        body_list.push_front(i);
        uint16_t idx_to_remove = body_list.front();
        if (i == 4) {
            body_list.pop_front();
        }
        for (int j = 0; j < 7; j++) {
            cout << body_list[j] << " ";
        }
        cout << endl;
    }
}


// worse than veque 
void devectors(uint16_t times) {
    rdsl::devector<uint16_t> veq = rdsl::devector<uint16_t>(5,0);

    for (uint16_t i = 1; i < times; i++) {
        veq.push_back(i);
        uint16_t idx_to_remove = veq.front();
        veq.pop_front();
    }
}

// doesnt work ? 
// void devectors_front(uint16_t times) {
//     rdsl::devector<uint16_t> veq = rdsl::devector<uint16_t>(5,0);

//     for (uint16_t i = 1; i < times; i++) {
//         veq.push_front(i);
//         uint16_t idx_to_remove = veq.back();
//         veq.pop_back();
//     }
// }


void veques(uint16_t times) { 
    veque::veque<uint16_t> veq = veque::veque<uint16_t>(5, 0);

    for (uint16_t i = 1; i < times; i++) {
        veq.push_back(i);
        uint16_t idx_to_remove = veq.pop_front_element();
    }
}

// seems to be best for now 
void veques_front(uint16_t times) { 
    veque::veque<uint16_t> veq = veque::veque<uint16_t>(5, 0);

    for (uint16_t i = 1; i < times; i++) {
        veq.push_front(i);
        uint16_t idx_to_remove = veq.pop_back_element();
    }
}

void veques_front_resize(uint16_t times) { 
    veque::veque<uint16_t> veq = veque::veque<uint16_t>(5, 0);
    uint16_t length = 5;
    for (uint16_t i = 1; i < times; i++) {
        length++;
        veq.reserve_front(1);
        veq.push_front(i);
        uint16_t idx_to_remove = veq.pop_back_element();
    }
}



// seems to beat out deques 
void veques_front_eating(uint16_t times) {
    veque::veque<uint16_t> veq = veque::veque<uint16_t>(5, 0);

    for (uint16_t i = 1; i < times; i++) {
        veq.push_front(i);
    }
}


void arrays(uint16_t times) {

    uint16_t len = 121;
    uint16_t arr[len] = {0};
    for (uint16_t i = 1; i < times; i++) {
        uint16_t idx_to_remove = arr[len - 1];
        // 0th index represents the square we just moved to ; last index is index to remove
        for (uint16_t j = len - 1; j > 0; j--) {
            arr[j] = arr[j-1];
        }
        arr[0] = i;
        // for (int j = 0; j < len; j++) {
        //     cout << arr[j] << " ";
        // }
        // cout << endl;
    }
}


void queues(uint16_t times) {
    // try with different sizes 
    deque<uint16_t> list = deque<uint16_t>(5, 0);

    for (uint16_t i = 1; i < times; i++) {
        list.push_front(i);
        uint16_t idx_to_remove = list.back();
        list.pop_back();
    }
}


// worse 
void queues_front(uint16_t times) {
    // try with different sizes 
    deque<uint16_t> list = deque<uint16_t>(5, 0);
    
    for (uint16_t i = 1; i < times; i++) {
        list.push_back(i);
        uint16_t idx_to_remove = list.front();
        list.pop_front();
    }
}


void vectors(uint16_t times) {
    vector<uint16_t> vec = vector<uint16_t>(5, 0);

    for (uint16_t i = 1; i < times; i++) {
        vec.insert(vec.begin(), i);
        uint16_t idx_to_remove = vec.back();
        vec.pop_back();
    }
}


void vectors_front(uint16_t times) {
    vector<uint16_t> vec = vector<uint16_t>(5, 0);

    for (uint16_t i = 1; i < times; i++) {
        vec.push_back(i);
        uint16_t idx_to_remove = vec.front();
        vec.erase(vec.begin());
    }
}


// worse 
// calling size() seems to be a big problem 
void queues_inplace(uint16_t times) {
    deque<uint16_t> list = deque<uint16_t> (5, 0);

    for (uint16_t i = 1; i < times; i++) {
        uint16_t idx_to_remove = list[4];
        for (uint16_t j = 4; j > 0; j--){
            list[j] = list[j - 1];
        }
        list[0] = i;
    }
}


// worse 
void arrays_eating(uint16_t times) {
uint16_t* arr = new uint16_t[1];
    arr[0] = 0;
    uint16_t len = 1;
    for (uint16_t i = 1; i < times; i++) {
        uint16_t* new_arr = new uint16_t[len + 1];
        new_arr[0] = i; // adding new head idx
        for (uint16_t j = 0; j < len; j++) {
            new_arr[j + 1] = arr[j];
        }
        len++;
        delete[] arr;
        arr = new_arr;
    }
}


// void array_eating(uint16_t times) {

//     uint16_t arr[] = {0};
//     uint16_t len = 1;
//     for (uint16_t i = 1; i < times; i++) {
//         uint16_t new_arr[len + 1];
//         new_arr[0] = i; // adding new head idx
//         for (uint16_t i=0 ; i < len + 1; i++) {
//             new_arr[i + 1] = arr[i];
//         }
//         len++;
//         arr = new_arr;
//     }
// }


void queues_eating(uint16_t times) {
    deque<uint16_t> list = deque<uint16_t> (1, 0);
    for (uint16_t i = 1; i < times; i++){
        list.push_front(i);
    }
}


void long_array(uint16_t times) {
    uint16_t arr[65540] = {0};
    uint16_t tail = 0;
    uint16_t head = 2;
    for (int i = 0; i < times; i++) {
        tail++;
        head++;
        arr[head] = i;
    }
}





int main() {

    uint16_t times = 65535;

    chrono::time_point<chrono::high_resolution_clock> start, end;
    double elapsed_time;


    start = chrono::high_resolution_clock::now();
    long_array(times);
    end = chrono::high_resolution_clock::now();
    elapsed_time = double(chrono::duration_cast <chrono::nanoseconds> (end - start).count());
    cout << "Array time nanoseconds: " << elapsed_time << endl;
    cout << fixed << "Array time seconds: " << elapsed_time / 1e9 << endl;

    // start = chrono::high_resolution_clock::now();
    // veques_front(times);
    // end = chrono::high_resolution_clock::now();
    // elapsed_time = double(chrono::duration_cast <chrono::nanoseconds> (end - start).count());
    // cout << "Queue time nanoseconds: " << elapsed_time << endl;
    // cout << fixed << "Queue time seconds: " << elapsed_time / 1e9 << endl;

    return 0;
}

// veque still wins both 