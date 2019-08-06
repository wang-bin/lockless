
/*
 * Copyright (c) 2017-2019 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * https://github.com/wang-bin/lock_free
 */
#include <atomic>
#include <utility>

template<typename T>
class fifo {
public:
    ~fifo() {
        clear();
        delete out_;
    }

    void clear() { while(pop()) {}} // in consumer thread

    template<typename... Args>
    void emplace(Args&&... args) {
        node *n = new node{std::forward<Args>(args)...};
        in_->next = n;
        in_ = n;
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
        in_->next = n;
        in_ = n;
    }

    bool pop(T* v = nullptr) {
        if (out_ == in_)
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
    node* in_ = out_;
};

