//
// Created by lukeypookster on 11/21/26.
//


#include "slipstream/engine.h"
#include "slipstream/config.h"

#include <poll.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstdio>

namespace {
    constexpr size_t kRecvChunk = 2048;

    template <typename Variant, typename Handler>
    bool pump(const FileDescriptor& conn, FrameReader<Variant>& reader, Handler handle) {
        std::byte chunk[kRecvChunk];
        const ssize_t n = ::recv(conn.get(), chunk, sizeof(chunk), 0);

        if (n == 0) return false;
        if (n < 0) {
            if (errno == EINTR) return true;
            std::perror("recv");
            return false;
        }

        if (!reader.feed(chunk, static_cast<size_t>(n))) {
            std::fprintf(stderr, "frame buffer overflow\n");
            return false;
        }

        Variant msg;
        DecodeResult r;
        while ((r = reader.next(msg)) != DecodeResult::Incomplete) {
            if (r == DecodeResult::Invalid) {
                std::fprintf(stderr, "invalid frame, dropping connection\n");
                return false;
            }
            if (r != DecodeResult::Ok) continue; // Unknown msg_type, frame already consumed
            handle(msg);
        }
        return true;
    }
}

Engine::Engine(const Config& cfg)
    : cfg_{cfg}, filter_{cfg_.symbol}, vwap_{cfg_.vwap_window_ms} {}

bool Engine::bind() {
    md_listener_ = listen_on(cfg_.md_port);
    oe_listener_ = listen_on(cfg_.oe_port);

    if (!md_listener_.valid() || !oe_listener_.valid()) return false;

    std::printf("slipstream: %s, md port %u, oe %u\n",
                cfg_.symbol.c_str(), cfg_.md_port, cfg_.oe_port);
    std::fflush(stdout);
    return true;
}

void Engine::run() {
    while (!done()) {
        pollfd fds[4]{};
        int n = 0;
        int md_listen_i = -1, oe_listen_i = -1, md_conn_i = -1, oe_conn_i = -1;

        if (md_ == LinkState::Listening) {
            fds[n] = {md_listener_.get(), POLLIN, 0};
            md_listen_i = n++;
        }
        if (oe_ == LinkState::Listening) {
            fds[n] = {oe_listener_.get(), POLLIN, 0};
            oe_listen_i = n++;
        }
        if (md_ == LinkState::Connected) {
            fds[n] = {md_conn_.get(), POLLIN, 0};
            md_conn_i = n++;
        }
        if (oe_ == LinkState::Connected) {
            fds[n] = {oe_conn_.get(), POLLIN, 0};
            oe_conn_i = n++;
        }

        const int ready = ::poll(fds, static_cast<nfds_t>(n), -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::perror("poll");
            return;
        }

        constexpr short kReadable = POLLIN | POLLHUP | POLLERR;

        if (md_listen_i >= 0 && (fds[md_listen_i].revents & POLLIN)) accept_md();
        if (oe_listen_i >= 0 && (fds[oe_listen_i].revents & POLLIN)) accept_oe();
        if (md_conn_i >= 0 && (fds[md_conn_i].revents & kReadable)) service_md();
        if (oe_conn_i >= 0 && (fds[oe_conn_i].revents & kReadable)) service_oe();
    }
}

void Engine::accept_md() {
    md_conn_ = accept_one(md_listener_);
    if (!md_conn_.valid()) {
        md_ = LinkState::Closed;
        return;
    }
    md_ = LinkState::Connected;
    std::printf("md client connected\n");
    std::fflush(stdout);
}

void Engine::accept_oe() {
    oe_conn_ = accept_one(oe_listener_);
    if (!oe_conn_.valid()) {
        oe_ = LinkState::Closed;
        return;
    }
    oe_ = LinkState::Connected;
    std::printf("oe client connected\n");
    std::fflush(stdout);
}

void Engine::service_md() {
    const bool alive = pump(md_conn_, md_reader_, [this](const MdMessage& msg) {
        if (const Quote* q = std::get_if<Quote>(&msg)) on_quote(*q);
    });
    if (alive) return;

    md_ = LinkState::Closed;
    md_conn_ = FileDescriptor{};
    std::printf("md client disconnected\n");
    std::fflush(stdout);
}

void Engine::service_oe() {
    const bool alive = pump(oe_conn_, oe_reader_, [this](const OeMessage& msg) {
        if (const Trade* t = std::get_if<Trade>(&msg)) on_trade(*t);
    });
    if (alive) return;

    oe_ = LinkState::Closed;
    oe_conn_ = FileDescriptor{};
    std::printf("oe client disconnected\n");
    std::fflush(stdout);
}

void Engine::on_quote(const Quote& q) {
    ++stats_.quotes_received;
    if (!filter_.matches(q.symbol)) return;
    ++stats_.quotes_kept;
    book_.apply(q);
}

void Engine::on_trade(const Trade& t) {
    ++stats_.trades_received;
    if (!filter_.matches(t.symbol)) return;
    ++stats_.trades_kept;
    vwap_.on_trade(t);
}

void Engine::report() const {
    std::printf("quotes received %zu, kept %zu\n", stats_.quotes_received, stats_.quotes_kept);
    std::printf("trades received %zu, kept %zu\n", stats_.trades_received, stats_.trades_kept);

    if (book_.valid) {
        std::printf("last book  bid %u @ %.4f   ask %u @ %.4f\n",
                    book_.bid_qty, static_cast<double>(book_.bid_px) / 10000.0,
                    book_.ask_qty, static_cast<double>(book_.ask_px) / 10000.0);
    }

    int64_t v = 0;
    if (vwap_.value(v)) {
        std::printf("last vwap  %.4f over %ld shares, %zu trades in window, warm %s\n",
                    static_cast<double>(v) / 10000.0, vwap_.window_qty(), vwap_.size(),
                    vwap_.has_full_window() ? "yes" : "no");
    }
}