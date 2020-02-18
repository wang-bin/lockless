
/*
 * Copyright (c) 2017-2020 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free SPSC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>

// namespace lockless { namespace spsc {}}
template<typename T>
class spsc_fifo {
public:
    spsc_fifo() { in_.store(out_); }

    ~spsc_fifo() {
        clear();
        delete out_;
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
        node* t = in_.load(std::memory_order_relaxed);
        t->next = n;
        in_.store(n, std::memory_order_release);
    }

    template<typename U>
    void push(U&& v) {
        node *n = new node{std::forward<U>(v)};
        node* t = in_.load(std::memory_order_relaxed);
        t->next = n;
        in_.store(n, std::memory_order_release); // ensure t->next is written
    }

    bool pop(T* v = nullptr) {
        if (out_ == in_.load(std::memory_order_acquire)) //if (!head->next) // not completely write to head->next (tail->next), next is not null but invalid
            return false;
        node *h = out_;
        out_ = h->next;
        if (v)
            *v = std::move(h->next->v);
        delete h;
        return true;
    }
private:
    struct node {
        T v;
        node *next;
    };

    node *out_ = new node();
    std::atomic<node*> in_; // can not use in_{out_} because atomic ctor with desired value MUST be constexpr (error in g++4.8 iff use template)
};

