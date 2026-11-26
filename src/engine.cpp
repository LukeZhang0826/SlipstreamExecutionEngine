//
// Created by lukeypookster on 11/21/26.
//


#include "slipstream/engine.h"
#include "slipstream/config.h"

#include <poll.h>
#include <sys/socket.h>
#include <time.h>

#include <algorithm>
#include <cstring>

#include <cerrno>
#include <cstdio>

namespace {
    constexpr size_t kRecvChunk = 2048;

    uint64_t now_ns() {
        timespec ts{};
        ::clock_gettime(CLOCK_REALTIME, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000 + static_cast<uint64_t>(ts.tv_nsec);
    }

    double as_price(const int64_t fixed) { return static_cast<double>(fixed) / 10'000.0; }
    double as_percent(const uint64_t ppm) { return static_cast<double>(ppm) / 10'000.0; }

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
    : cfg_{cfg}, filter_{cfg_.symbol}, vwap_{cfg_.vwap_window_ms}, strategy_{cfg_} {}

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
    vwap_.on_quote(q);
    strategy_.on_quote(q);
}

void Engine::on_trade(const Trade& t) {
    ++stats_.trades_received;
    if (!filter_.matches(t.symbol)) return;
    ++stats_.trades_kept;

    vwap_.advance(t.ts_ns);

    const Decision d = strategy_.judge(t, book_, vwap_);
    switch (d.verdict) {
        case Verdict::Suppress:
            ++stats_.suppressed;
            return;
        case Verdict::Accept:
            ++stats_.accepted;
            break;
        case Verdict::Reject:
            ++stats_.rejected;
            ++stats_.reject_reasons[static_cast<size_t>(d.reason)];
            break;
    }
    send_verdict(t, d);
}

void Engine::send_verdict(const Trade& t, const Decision& d) {
    if (oe_ != LinkState::Connected) return;

    NewOrder order{};
    order.client_order_id = ++next_order_id_;
    std::memcpy(order.symbol, cfg_.symbol.data(), std::min(cfg_.symbol.size(), sizeof(order.symbol)));

    order.status = d.verdict == Verdict::Accept ? 'A' : 'R';
    order.ts_ns = now_ns();
    order.trade_id = t.id;
    order.side = d.side;
    order.qty = d.qty;
    order.limit_px = d.limit_px;

    if (!send_oe(OeMessage{order})) {
        oe_ = LinkState::Closed;
        oe_conn_ = FileDescriptor{};
        std::printf("oe client write failed\n");
        std::fflush(stdout);
    }
}

bool Engine::send_oe(const OeMessage& msg) {
    std::byte buf[kMaxFrameLen];
    const size_t len = encode_frame(msg, buf, sizeof(buf));
    if (len == 0) {
        std::fprintf(stderr, "encode failed\n");
        return false;
    }

    size_t sent = 0;
    while (sent < len) {
        const ssize_t w = ::send(oe_conn_.get(), buf + sent, len - sent, MSG_NOSIGNAL);
        if (w < 0) {
            if (errno == EINTR) continue;
            std::perror("send");
            return false;
        }
        sent += static_cast<size_t>(w);
    }
    return true;
}

void Engine::report() const {
    std::printf("=== SLIPSTREAM EXECUTION REPORT ===\n");
    std::printf("symbol              %s\n", cfg_.symbol.c_str());
    std::printf("quotes              %zu observed, %zu for symbol\n",
                stats_.quotes_received, stats_.quotes_kept);
    std::printf("trades              %zu observed, %zu for symbol\n",
                stats_.trades_received, stats_.trades_kept);
    std::printf("accepted            %zu\n", stats_.accepted);
    std::printf("rejected            %zu\n", stats_.rejected);
    std::printf("pre warm up         %zu\n", stats_.suppressed);
    for (size_t i = 0; i < kRejectReasonCount; ++i) {
        if (stats_.reject_reasons[i] == 0) continue;
        std::printf("  %-24s  %zu\n", to_string(static_cast<RejectReason>(i)),
                    stats_.reject_reasons[i]);
    }
    std::printf("bought qty          %lu  of %lu offered on the ask\n",
                strategy_.bought_qty(), strategy_.observed_ask_qty());
    std::printf("sold qty            %lu  of %lu offered on the bid\n",
                strategy_.sold_qty(), strategy_.observed_bid_qty());
    std::printf("buy participation   %.2f%%  (cap %.2f%%)\n",
                as_percent(strategy_.buy_participation_ppm()),
                as_percent(cfg_.participation_cap_ppm));
    std::printf("sell participation  %.2f%%  (cap %.2f%%)\n",
                as_percent(strategy_.sell_participation_ppm()),
                as_percent(cfg_.participation_cap_ppm));

    if (book_.valid) {
        std::printf("last book           bid %u @ %.4f   ask %u @ %.4f\n",
                    book_.bid_qty, as_price(book_.bid_px),
                    book_.ask_qty, as_price(book_.ask_px));
    }

    int64_t v = 0;
    if (vwap_.value(v)) {
        std::printf("last vwap           %.4f over %zu quotes in window\n",
                    as_price(v), vwap_.size());
    }
}
