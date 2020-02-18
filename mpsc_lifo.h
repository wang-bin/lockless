/*
 * Copyright (c) 2017-2020 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPSC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>

template<typename T>
class mpsc_lifo {
public:
    ~mpsc_lifo() {
        clear();
    }

    // return number of element cleared
    int clear() {
        int n = 0;
        while (pop())
            n++;
        return n;
    } // in consumer thread

    template<typename... Args>
    void emplace(Args&&... args) {
        node *n = new node{std::forward<Args>(args)...};
        n->next = io_.load();
        while (!io_.compare_exchange_weak(n->next, n)) {}
        count_++;
    }

    template<typename U>
    void push(U&& v) {
        node *n = new node{std::forward<U>(v)};
        n->next = io_.load(); // next can be a raw ptr
        while (!io_.compare_exchange_weak(n->next, n)) {}
        count_++;
    }

    bool pop(T* v = nullptr) {
        node* out = io_.load(std::memory_order_relaxed);
        if (!out)
            return false;
        // io_ can be modified in push(), so compare is required
        while (!io_.compare_exchange_weak(out, out->next)) {}
        if (v)
            *v = std::move(out->v);
        delete out;
        count_--;
        return true;
    }

    int size() const {
        return count_;
    }
private:
    struct node {
        T v;
        node* next;
    };

    std::atomic<node*> io_{nullptr};
    std::atomic<int> count_{0};
};
