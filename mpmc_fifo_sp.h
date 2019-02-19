/*
 * Copyright (c) 2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPMC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>
#include <cstdio>

template<typename T>
class mpmc_fifo {
public:
    mpmc_fifo() {
        auto n = std::make_shared<node>();
        out_.store(n);
        in_.store(n);
    }

    ~mpmc_fifo() {
        clear();
    }

    // return number of element cleared
    int clear() {
        int n = 0;
        while (pop())
            n++;
        return n;
    } // in consumer thread

    void push(T&& v) {
        auto n = std::make_shared<node>(std::move(v));
        auto t = in_.exchange(n, std::memory_order_acq_rel);
        t->next.store(n, std::memory_order_release);
    }

    bool pop(T* v = nullptr) {
        // will check next.load() later, also next.store() in push() must be after exchange, so relaxed is enough
        auto out = out_.load(std::memory_order_relaxed);
        shared_ptr<node> n;
        do {
            if (out == in_.load(std::memory_order_relaxed)) // pop() by other consumer and now empty
                return false;
            n = out->next.load(std::memory_order_relaxed);
            if (!n)
                return false;
        } while (!out_.compare_exchange_weak(out, n));
        if (v)
            *v = std::move(n->v);
        return true;
    }
private:
// node storage[MAX]; atomic<int> pa; allocate()=> storage[index(pa++)/%MAX]
    struct node {
        T v;
        std::atomic<shared_ptr<node>> next;
        uint32_t id;
    };

    // TODO: aligas(hardware_destructive_interference_size)
    std::atomic<shared_ptr<node>> out_;
    std::atomic<shared_ptr<node>> in_;
};
