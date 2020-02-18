
/*
 * Copyright (c) 2017-2020 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * https://github.com/wang-bin/lockless
 */
#include <atomic>
#include <utility>

template<typename T>
class fifo {
public:
    ~fifo() {
        clear();
    }

    void clear() { while(pop()) {}} // in consumer thread

    template<typename... Args>
    void emplace(Args&&... args) {
        node *n = new node{std::forward<Args>(args)...};
        n->next = io_;
        io_ = n;
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
        n->next = io_;
        io_ = n;
    }

    bool pop(T* v = nullptr) {
        if (!io_)
            return false;
        node *out = io_;
        io_ = out->next;
        if (v)
            *v = std::move(out->v);
        delete out;
        return true;
    }
private:
    struct node {
        T v;
        node *next;
    };

    node *io_ = nullptr;;
};
