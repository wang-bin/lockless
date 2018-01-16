/*
 * Copyright (c) 2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPMC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>

#define MPMC_FIFO_RAW_NEXT_PTR 0 // raw ptr requires while(!compare_exchange...). FIXME: push wrror?

template<typename T>
class mpmc_fifo {
public:
    mpmc_fifo() {
        node* n = new node();
        out_.store(n);
        in_.store(n);
    }

    ~mpmc_fifo() {
        clear();
        delete out_.load();
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
#if MPMC_FIFO_RAW_NEXT_PTR // slower?
        node* t = in_.load(std::memory_order_relaxed);
        do {
            t->next = n;
        } while (!in_.compare_exchange_weak(t, n));
#else
        node* t = in_.exchange(n, std::memory_order_acq_rel);
        t->next.store(n, std::memory_order_release);
#endif
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
#if MPMC_FIFO_RAW_NEXT_PTR // slower
        node* t = in_.load(std::memory_order_relaxed);
        do {
            t->next = n;
        } while (!in_.compare_exchange_weak(t, n, std::memory_order_acq_rel, std::memory_order_relaxed));
#else
        node* t = in_.exchange(n, std::memory_order_acq_rel);
        t->next.store(n, std::memory_order_release);
#endif
    }

// recursive recycle
    bool pop(T* v = nullptr) {
        popping_++;
        // will check next.load() later, also next.store() in push() must be after exchange, so relaxed is enough
        node* out = out_.load(std::memory_order_relaxed);
        node* n = nullptr;
        do {
            if (out == in_.load(std::memory_order_relaxed)) {// pop() by other consumer and now empty
                popping_--;
                return false;
            }
            // if out is now deleted by another pop. atomic<shared_ptr<node>>?
#if MPMC_FIFO_RAW_NEXT_PTR
            n = out->next;
#else
            n = out->next.load(std::memory_order_relaxed);
#endif
            if (!n) {
                popping_--;
                return false;
            }
        } while (!out_.compare_exchange_weak(out, n));
        if (v)
            *v = std::move(n->v);
        try_delete(out);
        return true;
    }
private:
    struct node {
        T v;
#if MPMC_FIFO_RAW_NEXT_PTR
        node* next;
#else
        std::atomic<node*> next;
#endif
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
#if MPMC_FIFO_RAW_NEXT_PTR
        end->next = pendding_delete_;
        while (!pendding_delete_.compare_exchange_weak(end->next, begin)) {}
#else
        node* n = pendding_delete_.exchange(begin);
        end->next.store(n);
#endif
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

    // TODO: aligas(hardware_destructive_interference_size)
    std::atomic<node*> out_; // TODO: atomic<shared_ptr<node>> out_; get rid of memory management if lock free
    std::atomic<node*> in_; // can not use in_{out_} because atomic ctor with desired value MUST be constexpr (error in g++4.8 iff use template)
    std::atomic<int> popping_{0};
    std::atomic<node*> pendding_delete_{nullptr};
    //mpsc_fifo<node*> pendding_delete_; // unsafe to clear in 3 comsumer threads
    //mpmc_fifo<node*> reuse_; // mpmc_fifo<node*> *reuse_; // TODO: recursively reuse undeleted node in push() to slow down leak
};
