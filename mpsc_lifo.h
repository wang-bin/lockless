/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPSC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>

#define MPSC_LIFO_RAW_NEXT_PTR 0

template<typename T>
class mpsc_lifo {
public:
    ~mpsc_lifo() {
        clear();
    }

    void clear() { while(pop()) {}} // in consumer thread

    template<typename... Args>
    void emplace(Args&&... args) {
        node *n = new node{std::forward<Args>(args)...};
#if MPSC_LIFO_RAW_NEXT_PTR
        n->next = io_.load();
        while (!io_.compare_exchange_weak(n->next, n)) {}
#else
        node* t = io_.exchange(n, std::memory_order_acq_rel);
        n->next.store(t, std::memory_order_release);
#endif
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
#if MPSC_LIFO_RAW_NEXT_PTR
        n->next = io_.load(); // next can be a raw ptr
        while (!io_.compare_exchange_weak(n->next, n)) {}
#else
        node* t = io_.exchange(n, std::memory_order_acq_rel);
        n->next.store(t, std::memory_order_release);
#endif
    }

    bool pop(T* v = nullptr) {
        node* out = io_.load(std::memory_order_relaxed);
        if (!out)
            return false;
        // io_ can be modified in push(), so compare is required
#if MPSC_LIFO_RAW_NEXT_PTR
        while (!io_.compare_exchange_weak(out, out->next)) {}
#else
        node *n = nullptr;
        do {
            n = out->next.load(std::memory_order_relaxed);
            if (!n) // next.store() may be not executed
                return false;
        } while (!io_.compare_exchange_weak(out, n));
#endif
        if (v)
            *v = std::move(out->v);
        delete out;
        return true;
    }
private:
    struct node {
        T v;
#if MPSC_LIFO_RAW_NEXT_PTR
        node* next;
#else
        std::atomic<node*> next;
#endif
    };

    std::atomic<node*> io_;
};
