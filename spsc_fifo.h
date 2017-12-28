
/*
 * Copyright (c) 2017 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free SPSC FIFO
 */
#include <atomic>
#include <utility>

template<typename T>
class spsc_fifo {
public:
    spsc_fifo() { in_.store(out_); }

    ~spsc_fifo() {
        while (node* h = out_) {
            out_ = h->next;
            delete h;
        }
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
        node* t = in_.load(std::memory_order_relaxed);
        t->next = n;
        in_.store(n, std::memory_order_release);
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
        node *next = nullptr;
    };

    node *out_ = new node();
    std::atomic<node*> in_; // can not use in_{out_} because atomic ctor with desired value MUST be constexpr (error in g++4.8 iff use template)
};

