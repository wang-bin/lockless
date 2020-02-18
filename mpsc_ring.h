/*
 * Copyright (c) 2018-2020 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPSC Ring
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>
#include <vector>
#include "cppcompat/iterator.hpp"
// TODO: test apis

namespace lockless {
namespace mpsc { // policy?

template<typename T, typename C>
class ring_api {
public:
    void clear() { while (pop()) {}} // in consumer thread

    template<typename U>
    bool push(U&& t) {
        auto in = in_.load(std::memory_order_relaxed);
        // can not use exchange because depending on old in, producers may have the same old in value
        while (!in_.compare_exchange_weak(in, index(in+1))) {} // TODO: spsc exchange()
        data_[in] = std::forward<U>(t);
        return try_update_out_index(in);
    }

    template<typename... Args>
    bool emplace(Args&&... args) {
//        static_assert(std::is_array<C>::value, "only static_ring supports emplace(...)");
        auto in = in_.load(std::memory_order_relaxed);
        while (!in_.compare_exchange_weak(in, index(in+1))) {}
        data_[in_].~T(); // already default constructed, so destruct first
        new (&data_[in]) T{std::forward<Args>(args)...};
        return try_update_out_index(in);
    }

    int pop(T* v = nullptr) {
        auto out = out_.load(std::memory_order_relaxed);
        auto in = in_.load(std::memory_order_relaxed);
        if (out == in) // mpsc only?
            return 0;
        // push() may overlap out index
        while (!out_.compare_exchange_weak(out, index(out+1))) {
            in = in_.load(std::memory_order_relaxed);
        }
        if (v)
            *v = data_[out];
        data_[out] = T(); //erase the old data. can not ~T() because the uninitialized object may be used in front()
        return size(in, out);
    }

    int capacity() const { return int(std::size(data_) - 1); }
    int size() const { return size(in_.load(std::memory_order_relaxed), out_.load(std::memory_order_relaxed));}
    bool empty() const { return size() == 0;}
    const T &at(size_t i) const { return data_[index(out_+i)];}
    const T &operator[](size_t i) const { return at(i);}
    T &operator[](size_t i) { return data_[index(out_+i)];}
    template<typename F>
    void dump(F&& f) {
        for (const auto& i : data_) {
            f(i);
        }
    }
protected:
    int extent() const { return capacity() + 1; }
    int index(int i) const { return i < extent() ? i : i - extent();} // i is always in [0,extent())
    int size(int in, int out) const {
        const int i_o = in - out; // size_t diff is size_t, but we need signed int
        return i_o < 0 ? extent() + i_o : i_o;
    }
    bool try_update_out_index(int in) {
        auto out = out_.load(std::memory_order_relaxed);
        if (size(in, out) == capacity()) {
            while (!out_.compare_exchange_weak(out, index(out+1))) { // FIXME
                if (size(in_.load(std::memory_order_relaxed), out) < capacity())
                    break;
            }
            return false;
        }
        return true;
    }

//  [1, 2, ..., cap, extent]
    std::atomic<int> out_;
    std::atomic<int> in_;
    C data_;
};

template<typename T>
class ring : public ring_api<T, std::vector<T>> {
    using api = ring_api<T, std::vector<T>>;
    using api::data_; // why need this?
public:
    ring(size_t cap = 0) : ring_api<T, std::vector<T>>() {
        reserve(cap);
    }
    // resize: reserve space if necessary, and push elements to reach given size
    void resize(int value) {
        if (value > api::capacity())
            reserve(value);
        const auto n = api::size();
        // grow
        for (int i = 0; i < value - n; ++i)
            api::push(T());
        // shrink
        for (int i = 0; i < n - value; ++i)
            api::pop();
    }
    void reserve(size_t cap) {
        data_.reserve(cap + 1);
        data_.resize(cap + 1);
    }
};

template<typename T, int N>
class static_ring : public ring_api<T, T[N+1]> {
    using api = ring_api<T, T[N+1]>;
    using ring_api<T, T[N+1]>::data_; // why need this?
public:
    static_ring() : ring_api<T, T[N+1]>() {}
    // resize: push elements to reach given size. can not larger than capacity()
    void resize(int value) {
        assert(value <= api::capacity());
        const auto n = api::size();
        // grow
        for (int i = 0; i < value - n; ++i)
            api::push(T());
        // shrink
        for (int i = 0; i < n - value; ++i)
            api::pop();
    }
};
} // namespace mpsc
} // namespace lockless
