//
// Created by lukeypookster on 11/5/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_FRAME_READER_H
#define SLIPSTREAMEXECUTIONENGINE_FRAME_READER_H

#include <cstring>

#include "codec.h"

template <typename MessageVariant, size_t Capacity = 4096>
class FrameReader {
    static_assert(Capacity >= kMaxFrameLen);
public:
    [[nodiscard]] bool feed(const std::byte* data, size_t n) {
        if (n > sizeof(buf_) - len_) return false;
        std::memcpy(buf_ + len_, data, n);
        len_ += n;
        return true;
    }
    [[nodiscard]] DecodeResult next(MessageVariant& out) {
        size_t consumed{0};
        const DecodeResult r = decode_frame(buf_, len_, out, consumed);
        if (consumed > 0) {
            std::memmove(buf_, buf_ + consumed, len_ - consumed);
            len_ -= consumed;
        }
        return r;
    }

private:
    std::byte buf_[Capacity];
    size_t    len_{0};
};

#endif //SLIPSTREAMEXECUTIONENGINE_FRAME_READER_H
