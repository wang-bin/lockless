/*
 * Copyright (c) 2018-2019 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPMC Pool
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include "mpmc_lifo.h"

template<typename T, int PoolSize = 16>
class mpmc_pool { // consumer thread can be producer thread, so availble models are single thread, mpsc, mpmc
public:
    ~mpmc_pool() {
        clear();
    }

    void set_deleter(std::function<void(T*)> deleter) {
        deleter_ = deleter;
    }

    int clear() {
        int n = 0;
        for (auto& p : fixed_pool_) {
            if (p.v) {
                deleter_(p.v); //
                p.v = nullptr;
                n++;
            }
        }
        T* t = nullptr;
        while (pool_.pop(&t)) {
            deleter_(t);
            n++;
        }
        return n;
    } // in consumer thread

    /*!
      \brief get
      fetch an object from pool, or create one if pool is empty
      \param f object T creator
      \param args... parameters of f
      \return unique_ptr of T
     */
    using tracked_ptr = std::unique_ptr<T, std::function<void(T*)>>;
    template<typename F, typename... Args>
    auto get(F&& f, Args&&... args) const->tracked_ptr {
        T* t = nullptr;
        if (!pool_.pop(&t)) {
            printf("LIFO pool is empty. create a new one\n");
            t = f(std::forward<Args>(args)...);
        }
        assert(t && "t can't be null");
        return {t, [this](T* t){
                pool_.push(std::move(t));
            }};
    }

    // try to get from static pool, and fallback to get() using lock free lifo
    template<typename F, typename... Args>
    auto get2(F&& f, Args&&... args) const->std::unique_ptr<T, std::function<void(T*)>> {
        for (auto& p : fixed_pool_) {
            if (!p.used.test_and_set()) {
                if (!p.v) // safe to check and init because only 1 thread can use it
                    p.v = f(std::forward<Args>(args)...);
                return {p.v, [&p](T* t){ p.used.clear();}};
            }
        }
        return get(std::forward<F>(f), std::forward<Args>(args)...);
    }
private:
    mutable mpmc_lifo<T*> pool_;
    using fixed_pool_node = struct {
        T* v = nullptr;
        std::atomic_flag used = ATOMIC_FLAG_INIT;
    };
    mutable fixed_pool_node fixed_pool_[PoolSize];
    std::function<void(T*)> deleter_ = std::default_delete<T>();
};
