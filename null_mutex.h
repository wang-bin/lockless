/*
 * Copyright (c) 2019 WangBin <wbsecg1 at gmail.com>
 */
#pragma once

struct null_mutex {
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};
