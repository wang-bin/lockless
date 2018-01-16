/*
 * Copyright (c) 2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * https://github.com/wang-bin/lockless
 */

#include "spsc_fifo.h"
#include "mpsc_fifo.h"
#include "mpmc_fifo.h"
#include <thread>
#include <iostream>
#include <chrono>

using namespace std;
using namespace chrono;

#define TEST(expr) do { \
        if (!(expr)) { \
                std::cerr << __LINE__ << " test error: " << #expr << std::endl; \
                exit(1); \
        } \
} while(false)

struct X {
    /*X(int x = 0, float y = 0){}
    X(X&&) =default;
    X& operator=(X&&) = default;*/
    int a;
    float b;
};

static const int N = 100000;
static const int NT = 4;

bool test_spsc_push_count() {
    cout << "testing spsc push count..." << std::endl;
    spsc_fifo<X> ss;
    for (int i = 0; i < N; ++i)
        ss.emplace(i, float(i));
    return ss.clear() == N;
}

bool test_spsc_rw() {
    cout << "testing spsc rw..." << std::endl;
    spsc_fifo<X> ss;
    thread tssp([&ss]{
        for (int i = 0; i < N; ++i)
            ss.emplace(i, float(i));
    });
    //this_thread::sleep_for(chrono::milliseconds(1));
    thread tssc([&ss]{
        for (int i = 0; i < N; ++i) {
            X x;
            if (!ss.pop(&x)) {
                //std::cout << this_thread::get_id() << " spsc_fifo.pop() failed @" << i << std::endl;
            }
        }
    });
    tssc.join();
    tssp.join();
    printf("pop fail count: %d\n", ss.clear());
    return true;
}

bool test_mpsc_push_count() {
    cout << "testing mpsc push count..." << std::endl;
    mpsc_fifo<X> ms;
    thread tmsp[NT];
    for (int k = 0; k < NT; ++k) {
        tmsp[k] = thread([&ms]{
            for (int i = 0; i < N; ++i)
                ms.emplace(i, float(i));
        });
    }
    for (auto& t: tmsp)
        t.join();
    return ms.clear() == NT*N;
}

bool test_mpsc_rw() {
    cout << "testing mpsc rw..." << std::endl;
    mpsc_fifo<X> ms;
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
                //std::cout << this_thread::get_id() << " mpsc_fifo.pop() failed @" << i << " count: " << ++c << std::endl;
            }
        }
    });
    tmsc.join();
    for (auto& t : tmsp)
        t.join();
    printf("pop fail count: %d\n", ms.clear());
    return true;
}

bool test_mpmc_push_count() {
    cout << "testing mpmc push count..." << std::endl;
    mpmc_fifo<X> mm;
    thread tmmp[NT];
    for (int k = 0; k < NT; ++k) {
        tmmp[k] = thread([&mm]{
            for (int i = 0; i < N; ++i)
                mm.emplace(i, float(i));
        });
    }
    for (auto& t: tmmp)
        t.join();
    return mm.clear() == N*NT;
}

bool test_mpmc_rw() {
    cout << "testing mpmc rw..." << std::endl;
    mpmc_fifo<X> mm;
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
                    //break;
                }
            }
        });
    }
    for (auto& t : tmmc)
        t.join();
    for (auto& t : tmmp)
        t.join();
    printf("pop fail count: %d\n", mm.clear());
    return true;
}

int main()
{
    X *x = new X{1,2.0f};
    spsc_fifo<X> q;
    q.emplace(1, 2.0f);
    q.push({2, 3.f});
    while (q.pop()) {
        printf("poped\n");
    }

    auto t0 = steady_clock::now();
    TEST(test_spsc_push_count());
    std::cout << "ms elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();
    TEST(test_spsc_rw());
    std::cout << "ms elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();
    TEST(test_mpsc_push_count());
    std::cout << "ms elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();
    TEST(test_mpsc_rw());
    std::cout << "ms elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();
    TEST(test_mpmc_push_count());
    std::cout << "ms elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();
    TEST(test_mpmc_rw());
    std::cout << "ms elapsed: " << duration_cast<milliseconds>(steady_clock::now() - t0).count() << std::endl;
    t0 = steady_clock::now();
    return 0;
}