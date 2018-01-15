/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * https://github.com/wang-bin/lockless
 */

#include "mpsc_lifo.h"
#include "mpmc_lifo.h"
#include <thread>
#include <iostream>
#include <chrono>

using namespace std;
using namespace chrono;

struct X {
    /*X(int x = 0, float y = 0){}
    X(X&&) =default;
    X& operator=(X&&) = default;*/
    int a;
    float b;
};

static const int N = 2000000;
static const int NT = 4;
int main()
{
    cout << "testing mpsc..." << std::endl;
    mpsc_lifo<X> ms;
    ms.push({1, 1.0f});
    while (ms.pop()) {
        printf("poped\n");
    }

    auto t0 = steady_clock::now();
    thread tmsp[NT];
    for (int k = 0; k < NT; ++k) {
        tmsp[k] = thread([&ms]{
            for (int i = 0; i < N; ++i)
                ms.emplace(i, float(i));
        });
    }
    thread tmsc([&ms]{
        static int c = 0;
        for (int i = 0; i < N*NT; ++i) {
            X x;
            if (!ms.pop(&x)) {
                //std::cout << this_thread::get_id() << " mpsc_lifo.pop() failed @" << i << " count: " << ++c << std::endl;
            }
        }
    });
    tmsc.join();
    for (auto& t : tmsp)
        t.join();
    ms.clear();
    std::cout << "elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();

    cout << "testing mpmc..." << std::endl;
    mpmc_lifo<X> mm;
    thread tmmp[NT];
    for (int k = 0; k < NT; ++k) {
        tmmp[k] = thread([&mm]{
            for (int i = 0; i < N; ++i)
                mm.emplace(i, float(i));
        });
    }
    thread tmmc[NT];
    for (int k = 0; k < NT; ++k) {
        tmmc[k] = thread([&mm]{
            for (int i = 0; i < N; ++i) {
                X x;
                if (!mm.pop(&x)) {
                    //std::cout << this_thread::get_id() << " mpmc pop failed @" << i << std::endl;
                }
            }
        });
    }
    for (auto& t : tmmc)
        t.join();
    for (auto& t : tmmp)
        t.join();
    mm.clear();
    std::cout << "elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
}