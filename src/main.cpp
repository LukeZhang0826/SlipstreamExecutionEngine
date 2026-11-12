#include <iostream>

#include "slipstream/messages.h"
#include "slipstream/codec.h"

#include "slipstream/net.h"
#include "slipstream/frame_reader.h"
#include <sys/socket.h>


namespace {
    constexpr size_t kRecvChunk = 2048;

    size_t run_md_session(const FileDescriptor& conn) {
        FrameReader<MdMessage> reader;
        std::byte chunk[kRecvChunk];
        size_t quotes = 0;

        while (true) {
            const ssize_t n = ::recv(conn.get(), chunk, sizeof(chunk), 0);

            if (n == 0) break;
            if (n < 0) {
                if (errno == EINTR) continue;
                std::perror("recv");
                break;
            }

            if (!reader.feed(chunk, static_cast<size_t>(n))) {
                std::fprintf(stderr, "frame buffer overflow\n");
                break;
            }

            MdMessage msg;
            bool draining = true;
            while (draining) {
                switch (reader.next(msg)) {
                    case DecodeResult::Ok:
                        if (std::get_if<Quote>(&msg)) ++quotes;
                        break;
                    case DecodeResult::Unknown:
                        break;
                    case DecodeResult::Invalid:
                        std::fprintf(stderr, "invalid frame, dropping connection\n");
                        return quotes;
                    case DecodeResult::Incomplete:
                        draining = false;
                        break;
                }
            }
        }
        return quotes;
    }

}

int main() {
    const FileDescriptor listener = listen_on(14200);
    if (!listener.valid()) return 1;
    std::printf("listening on 14200\n");

    const FileDescriptor conn = accept_one(listener);
    if (!conn.valid()) return 1;
    std::printf("md client connected\n\n");
    std::fflush(stdout);

    const size_t quotes = run_md_session(conn);
    std::printf("quotes: %zu\n", quotes);
    return 0;
}