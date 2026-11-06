//
// Created by lukeypookster on 10/17/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_BYTES_H
#define SLIPSTREAMEXECUTIONENGINE_BYTES_H

#include <cstddef>
#include <cstring>
#include <bits/ios_base.h>

class ByteWriter {
public:
    ByteWriter(std::byte* out, const size_t cap) : cur{out}, begin{out}, cap{cap} {}

    void put(const void* src, const size_t n) {
        if (!ok_) return;
        if (n > cap - written()) { ok_ = false; return; }
        std::memcpy(cur, src, n);
        cur += n;
    }

    [[nodiscard]] bool   ok() const      { return ok_; }
    [[nodiscard]] size_t written() const { return static_cast<size_t>(cur - begin); }

private:
    std::byte* cur;
    std::byte* begin;
    size_t     cap;
    bool       ok_{true};
};

class ByteReader {
public:
    ByteReader(const std::byte* in, const size_t len)
        : cur{in}, begin{in}, cap{len} {}

    void get(void* dest, const size_t n) {
        if (!ok_) return;
        if (n > cap - read()) { ok_ = false; return; }
        std::memcpy(dest, cur, n);
        cur += n;
    }

    [[nodiscard]] bool   ok() const   { return ok_; }
    [[nodiscard]] size_t read() const { return static_cast<size_t>(cur - begin); }

private:
    const std::byte* cur;
    const std::byte* begin;
    size_t           cap;
    bool             ok_{true};
};

#endif //SLIPSTREAMEXECUTIONENGINE_BYTES_H