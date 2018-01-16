/*
 * Copyright (c) 2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPMC Pool
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <cassert>
#include <functional>
#include <memory>
#include "mpmc_lifo.h"

template<typename T>
class mpmc_pool { // consumer thread can be producer thread, so availble models are single thread, mpsc, mpmc
public:
    ~mpmc_pool() {
        clear();
    }

    int clear() {
        T* t = nullptr;
        int n = 0;
        while (pool_.pop(&t)) {
            delete t;
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
    template<typename F, typename... Args>
    auto get(F&& f, Args&&... args) const->std::unique_ptr<T, std::function<void(T*)>> {
        T* t = nullptr;
        if (!pool_.pop(&t)) {
            printf("pool is empty. create a new one\n");
            t = f(args...);
        }
        assert(t && "t can't be null");
        return {t, [this](T* t){
                pool_.push(std::move(t));
            }};
    }
private:
    mutable mpmc_lifo<T*> pool_;
};
