/******************************************************************************
    QtAV:  Media play library based on Qt and FFmpeg
    Copyright (C) 2015-2020 Wang Bin <wbsecg1@gmail.com>

*   This file is part of QtAV

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#pragma once
#include <cassert>
#include <vector>
#include <iostream>
#include "cppcompat/iterator.hpp"
#include "null_mutex.h"
#include <mutex>
//https://github.com/WG21-SG14/SG14/tree/master/Docs/Proposals
//https://github.com/WG21-SG14/SG14/blob/master/SG14/ring.h

template<typename T, typename C, class Mutex>
class ring_api {
public:
    void clear() { while (pop_front()) {}}

    template<typename U>
    void push(U&& t) {
        std::lock_guard<Mutex> lock(mtx_);
        data_[in_] = std::forward<U>(t);
        update_index_after_push();
    }

    template<typename... Args>
    void emplace(Args&&... args) {
        std::lock_guard<Mutex> lock(mtx_);
        //static_assert(std::is_array<C>::value, "only static_ring supports emplace(...)");
        data_[in_].~T(); // already default constructed, so destruct first
        new (&data_[in_]) T{std::forward<Args>(args)...};
        update_index_after_push();
    }

// stl compatible
    void push_back(const T &t) { push(t);}
    void push_back(T&& t) { push(std::move(t));}
    template<typename... Args>
    void emplace_back(Args&&... args) { emplace(std::forward<Args>(args)...);}

    size_t pop(T* v = nullptr) {
        std::lock_guard<Mutex> lock(mtx_);
        const size_t n = size();
        if (n == 0)
            return n;
        if (v)
            *v = data_[out_];
        // FIXME: openal crash (use after free) if destroy old data
        //data_[out_] = T(); //erase the old data. can not ~T() because the uninitialized object may be used in front()
        //data_[out_].~T(); //erase the old data. can not ~T() because the uninitialized object may be used in front() and dtor
        //if (!std::is_array<C>::value)
          //  new (&data_[out_]) T();
        out_ = index(out_+1);
        return n;
    }
    bool pop_front() { return pop() > 0; }

    T &front() {
        //std::lock_guard<Mutex> lock(mtx_);
        return data_[out_];
    }

    const T &front() const {
        std::lock_guard<Mutex> lock(mtx_);
        return data_[out_];
    }
    T &back() {
        //std::lock_guard<Mutex> lock(mtx_);
        return data_[in_];
    }

    const T &back() const {
        std::lock_guard<Mutex> lock(mtx_);
        return data_[in_];
    }
    size_t capacity() const { return std::size(data_) - 1; }
    size_t size() const {
        return in_ < out_ ? extent() - (out_ - in_) : in_ - out_;
    }
    bool empty() const { return size() == 0;}
    // need at() []?
    const T &at(size_t i) const {
        std::lock_guard<Mutex> lock(mtx_);
        return data_[index(out_+i)];
    }

    const T &operator[](size_t i) const {return at(i);}
    T &operator[](size_t i) {
        std::lock_guard<Mutex> lock(mtx_);
        return data_[index(out_+i)];
    }

    template<typename F>
    void dump(F&& f) {
        for (const auto& i : data_)
            f(i);
    }

    size_t index_in() const {return in_;}
    size_t index_out() const {return out_;}
protected:
    size_t extent() const { return capacity() + 1; }
    size_t index(size_t i) const { return i < extent() ? i : i - extent();} // i is always in [0,extent())
    void update_index_after_push() {
        if (size() == capacity()) {
            std::clog << capacity() << " overwrite queued data. in " << in_ << " out_ " << out_ << std::endl;
            out_ = index(out_+1);
        }
        in_ = index(in_+1);
    }

//  [1, 2, ..., cap, extent]
    size_t out_ = 0; //size_ is simpler
    size_t in_ = 0;
    C data_;
    Mutex mtx_;
};

template<typename T, class Mutex = null_mutex>
class ring : public ring_api<T, std::vector<T>, Mutex> {
    using api = ring_api<T, std::vector<T>, Mutex>;
    using api::data_; // why need this?
    // store data here, construct ring_api with data begin/end like ring_span, data must be contiguous
public:
    ring(size_t cap = 0) : api() {
        reserve(cap);
    }
    // resize: reserve space if necessary, and push elements to reach given size
    void resize(int value) {
        if (value > (int)api::capacity())
            reserve(value);
        const auto n = (int)api::size();
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

template<typename T, int N, class Mutex = null_mutex>
class static_ring : public ring_api<T, T[N+1], Mutex> {
    using api = ring_api<T, T[N+1], Mutex>;
    using api::data_; // why need this?
public:
    static_ring() : api() {}
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
