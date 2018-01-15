/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * https://github.com/wang-bin/lockless
 */

#include "spsc_fifo.h"
#include "mpsc_fifo.h"
#include "mpmc_fifo.h"
#include <thread>
#include <iostream>

using namespace std;

struct X {
    /*X(int x = 0, float y = 0){}
    X(X&&) =default;
    X& operator=(X&&) = default;*/
    int a;
    float b;
};

static const int N = 100000;
static const int NT = 4;
int main()
{
    X *x = new X{1,2.0f};
    spsc_fifo<X> q;
    q.emplace(1, 2.0f);
    q.push({2, 3.f});

    cout << "testing spsc..." << std::endl;
    spsc_fifo<X> ss;
    thread tssp([&ss]{
        for (int i = 0; i < N; ++i)
            ss.emplace(i, float(i));
    });
    thread tssc([&ss]{
        for (int i = 0; i < N; ++i) {
            X x;
            if (!ss.pop(&x)) {
                std::cout << this_thread::get_id() << " spsc_fifo.pop() failed @" << i << std::endl;
            }
        }
    });
    tssp.join();
    tssc.join();
    ss.clear();
    cout << "testing mpsc..." << std::endl;
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
                std::cout << this_thread::get_id() << " mpsc_fifo.pop() failed @" << i << " count: " << ++c << std::endl;
            }
        }
    });
    tmsc.join();
    for (auto& t : tmsp)
        t.join();
    ms.clear();

    cout << "testing mpmc..." << std::endl;
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
                    std::cout << this_thread::get_id() << " mpmc pop failed @" << i << std::endl;
                }
            }
        });
    }
    for (auto& t : tmmc)
        t.join();
    for (auto& t : tmmp)
        t.join();
    mm.clear();
}