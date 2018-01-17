/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPSC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>

template<typename T>
class mpmc_lifo {
public:
    ~mpmc_lifo() {
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
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
        n->next = io_.load();
        while (!io_.compare_exchange_weak(n->next, n)) {}
    }

    bool pop(T* v = nullptr) {
        popping_++;
        node* out = io_.load(); // pop() in another thread & out can be null
        while (out && !io_.compare_exchange_weak(out, out->next)) {}
        if (!out) {// became empty in another thead pop()
            popping_--;
            return false;
        }
        if (v)
            *v = std::move(out->v);
        try_delete(out);
        return true;
    }
private:
    struct node {
        T v;
        node* next;
    };

    void delete_pendding(node* n) {
        while (n) {
            node *next = n->next;
            delete n;
            n = next;
        }
    }

    void try_delete(node* n) {
        if (popping_ == 1) {
            node* ns = pendding_delete_.exchange(nullptr);
            if (!--popping_) // safe, another thread run into pop() now will not get a deleting out
                delete_pendding(ns);
            else // 3 threads, t1 in try_delete before exchange(), t2  t3 just load() in pop() and get the same out, t3 finishes pop() first, t1 exchange and get t3 popped node, now popping_ is 2, if delete --popping>0, t2 later accesses invalid out 
                delete_later(ns); // can not delete (delete later appended reading node, so considering 3 threads is enough)
            delete n;
        } else {
            delete_later(n, n);
            popping_--;
        }
    }

    void delete_later(node* begin, node* end) {
        end->next = pendding_delete_;
        while (!pendding_delete_.compare_exchange_weak(end->next, begin)) {}
    }
    void delete_later(node* n) {
        if (!n)
            return;
        node* end = n;
        while (node* const next = end->next) {
            end = next;
        }
        delete_later(n, end);
    }

// = {} not {}: fix g++4.8 atomic copy ctor error in compiler generated default ctor if atomic member is direct list initialized (class template only)
    std::atomic<node*> io_ = {nullptr};
    std::atomic<node*> pendding_delete_ = {nullptr};
    std::atomic<int> popping_ = {0};
};
