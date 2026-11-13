#include <iostream>

#include "slipstream/messages.h"
#include "slipstream/codec.h"
#include "slipstream/book.h"
#include "slipstream/net.h"
#include "slipstream/frame_reader.h"
#include <sys/socket.h>

#include "slipstream/config.h"


namespace {
    constexpr size_t kRecvChunk = 2048;

    struct MdStats {
        size_t received = 0;
        size_t kept = 0;
    };

    MdStats run_md_session(const FileDescriptor& conn, const SymbolFilter& filter, L1& book) {
        FrameReader<MdMessage> reader;
        std::byte chunk[kRecvChunk];
        MdStats stats;

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
            DecodeResult r;
            while ((r = reader.next(msg)) != DecodeResult::Incomplete) {
                if (r == DecodeResult::Invalid) {
                    std::fprintf(stderr, "invalid frame, dropping connection\n");
                    return stats;
                }
                if (r != DecodeResult::Ok) continue; // DecodeResult::Unknown

                if (const Quote* q = std::get_if<Quote>(&msg)) {
                    ++stats.received;
                    if (!filter.matches(q->symbol)) continue;
                    ++stats.kept;
                    book.apply(*q);
                }
            }
        }
        return stats;
    }

}

int main(int argc, char** argv) {
    Config cfg;
    if (!parse_args(argc, argv, cfg)) return 2;

    const FileDescriptor listener = listen_on(cfg.md_port);
    if (!listener.valid()) return 1;
    std::printf("slipstream: %s, md port %u\n", cfg.symbol.c_str(), cfg.md_port);
    std::fflush(stdout);

    const FileDescriptor conn = accept_one(listener);
    if (!conn.valid()) return 1;

    const SymbolFilter filter{cfg.symbol};
    L1 book;
    const MdStats stats = run_md_session(conn, filter, book);

    std::printf("quotes received %zu, kept %zu\n", stats.received, stats.kept);
    if (book.valid) {
        std::printf("last book  bid %u @ %.4f   ask %u @ %.4f\n",
                    book.bid_qty, static_cast<double>(book.bid_px) / 10000.0,
                    book.ask_qty, static_cast<double>(book.ask_px) / 10000.0);
    }
    return 0;
}