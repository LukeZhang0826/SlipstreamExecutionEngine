//
// Created by lukeypookster on 11/21/26.
//


#include "slipstream/engine.h"
#include "slipstream/config.h"

#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <cctype>

#include <cerrno>
#include <cstdio>

namespace {
    constexpr size_t kRecvChunk = 2048;
    constexpr uint64_t kHeartbeatNs = 5'000'000'000;

    constexpr uint8_t kSessionOpen = 0;
    constexpr uint8_t kSessionHalt = 1;
    constexpr uint8_t kSessionClose = 2;

    constexpr size_t kMaxCommandLen = 1024;

    uint64_t clock_ns(const clockid_t clock) {
        timespec ts{};
        ::clock_gettime(clock, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000
             + static_cast<uint64_t>(ts.tv_nsec);
    }

    // Elapsed time
    uint64_t mono_ns() { return clock_ns(CLOCK_MONOTONIC); }

    // wire's ts_ns.
    uint64_t now_ns() { return clock_ns(CLOCK_REALTIME); }

    double as_price(const int64_t fixed) { return static_cast<double>(fixed) / 10'000.0; }
    double as_percent(const uint64_t ppm) { return static_cast<double>(ppm) / 10'000.0; }

    int64_t slippage_bps_x100(const int64_t fill_px, const int64_t benchmark) {
        return (fill_px - benchmark) * 1'000'000 / benchmark;
    }

    double as_bps(const int64_t bps_x100) { return static_cast<double>(bps_x100) / 100.0; }

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
        pollfd fds[5]{};
        int n = 0;
        int md_listen_i = -1, oe_listen_i = -1, md_conn_i = -1, oe_conn_i = -1, stdin_i = -1;

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
        if (stdin_open_) {
            fds[n] = {STDIN_FILENO, POLLIN, 0};
            stdin_i = n++;
        }

        const int ready = ::poll(fds, static_cast<nfds_t>(n), poll_timeout_ms());
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
        if (stdin_i >= 0 && (fds[stdin_i].revents & (POLLIN | POLLHUP))) service_stdin();

        check_oe_staleness();
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
    oe_idle_since_ns_ = mono_ns();

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
    end_session();
}

void Engine::service_oe() {
    const bool alive = pump(oe_conn_, oe_reader_, [this](const OeMessage& msg) {
        if (const Trade* t = std::get_if<Trade>(&msg)) on_trade(*t);
    });

    if (alive) {
        oe_idle_since_ns_ = mono_ns();
        return;
    }

    oe_ = LinkState::Closed;
    oe_conn_ = FileDescriptor{};
    std::printf("oe client disconnected\n");
    std::fflush(stdout);
    end_session();
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
    stats_.offered_qty += t.qty;

    vwap_.advance(t.ts_ns);

    if (session_ == SessionState::Halted) {
        ++stats_.halted;
        return;
    }

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

void Engine::end_session() {
    if (md_ == LinkState::Connected) {
        std::printf("closing md link\n");
        md_conn_ = FileDescriptor{};
    }
    if (oe_ == LinkState::Connected) {
        std::printf("closing oe link\n");
        oe_conn_ = FileDescriptor{};
    }
    md_ = LinkState::Closed;
    oe_ = LinkState::Closed;
}

int Engine::poll_timeout_ms() const {
    if (oe_ != LinkState::Connected) return -1;

    const uint64_t deadline = oe_idle_since_ns_ + kHeartbeatNs;
    const uint64_t now = mono_ns();

    if (now >= deadline) return 0;

    return static_cast<int>((deadline - now) / 1'000'000) + 1;
}

void Engine::check_oe_staleness() {
    if (oe_ != LinkState::Connected) return;

    const uint64_t now = mono_ns();
    const uint64_t quiet_ns = now - oe_idle_since_ns_;
    if (quiet_ns < kHeartbeatNs) return;

    std::fprintf(stderr, "oe link silent for %lu ms, sending heartbeat\n", quiet_ns / 1'000'000);

    Heartbeat hb{};
    hb.ts_ns = now_ns();
    if (!send_oe(OeMessage{hb})) {
        std::printf("oe client write failed\n");
        std::fflush(stdout);
        end_session();
        return;
    }
    oe_idle_since_ns_ = now;

}

void Engine::service_stdin() {
    char chunk[256];
    const ssize_t n = ::read(STDIN_FILENO, chunk, sizeof(chunk));
    if (n <= 0) {
        stdin_open_ = false;
        return;
    }

    stdin_line_.append(chunk, static_cast<size_t>(n));

    for (size_t nl = stdin_line_.find('\n'); nl != std::string::npos; nl = stdin_line_.find('\n')) {
        handle_command(std::string_view{stdin_line_}.substr(0, nl));
        stdin_line_.erase(0, nl + 1);
        if (done()) return;
    }

    if (stdin_line_.size() > kMaxCommandLen) stdin_line_.clear();
}

void Engine::handle_command(std::string_view line) {
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
        line.remove_prefix(1);
    }
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
        line.remove_suffix(1);
    }
    if (line.empty()) return;

    std::string cmd;
    cmd.reserve(line.size());
    for (const char ch : line) {
        cmd.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    if (cmd == "HALT") {
        if (session_ == SessionState::Halted) {
            std::printf("already halted\n");
        } else {
            session_ = SessionState::Halted;
            announce_session(kSessionHalt);
            std::printf("HALT: new orders suppressed, benchmark still updating\n");
        }
    } else if (cmd == "OPEN") {
        if (session_ == SessionState::Open) {
            std::printf("already open\n");
        } else {
            session_ = SessionState::Open;
            announce_session(kSessionOpen);
            std::printf("OPEN: sending orders again\n");
        }
    } else if (cmd == "CLOSE") {
        announce_session(kSessionClose);
        std::printf("CLOSE: closing both links\n");
        end_session();
    } else {
        std::printf("unknown command '%.*s', expected HALT, OPEN or CLOSE\n",
                    static_cast<int>(cmd.size()), cmd.data());
    }
    std::fflush(stdout);
}

void Engine::announce_session(const uint8_t state) {
    if (oe_ != LinkState::Connected) return;

    SessionControl sc{};
    sc.ts_ns = now_ns();
    sc.state = state;

    if (!send_oe(OeMessage{sc})) {
        std::printf("oe client write failed\n");
        std::fflush(stdout);
        end_session();
    }
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
    if (stats_.halted > 0) std::printf("while halted        %zu\n", stats_.halted);
    for (size_t i = 0; i < kRejectReasonCount; ++i) {
        if (stats_.reject_reasons[i] == 0) continue;
        std::printf("  %-24s  %zu\n", to_string(static_cast<RejectReason>(i)),
                    stats_.reject_reasons[i]);
    }

    const uint64_t executed_qty = strategy_.bought_qty() + strategy_.sold_qty();
    std::printf("offered qty         %lu\n", stats_.offered_qty);
    std::printf("executed qty        %lu", executed_qty);
    if (stats_.offered_qty > 0) {
        std::printf("   (%.2f%%)",
                    100.0 * static_cast<double>(executed_qty)
                          / static_cast<double>(stats_.offered_qty));
    }
    std::printf("\n");

    int64_t session_v = 0;
    const bool have_session_v = vwap_.session_value(session_v);
    if (have_session_v) std::printf("session vwap        %.4f\n", as_price(session_v));

    // Favorable means opposite things per side: a buy wants to pay below the
    // benchmark, a sell wants to receive above it.
    int64_t avg_buy = 0;
    if (strategy_.avg_buy_px(avg_buy)) {
        std::printf("avg buy fill        %.4f over %lu shares\n",
                    as_price(avg_buy), strategy_.bought_qty());
        if (have_session_v) {
            const int64_t slip = slippage_bps_x100(avg_buy, session_v);
            std::printf("buy slippage        %+.2f bps   (%s)\n",
                        as_bps(slip), slip <= 0 ? "favorable" : "unfavorable");
        }
    }

    int64_t avg_sell = 0;
    if (strategy_.avg_sell_px(avg_sell)) {
        std::printf("avg sell fill       %.4f over %lu shares\n",
                    as_price(avg_sell), strategy_.sold_qty());
        if (have_session_v) {
            const int64_t slip = slippage_bps_x100(avg_sell, session_v);
            std::printf("sell slippage       %+.2f bps   (%s)\n",
                        as_bps(slip), slip >= 0 ? "favorable" : "unfavorable");
        }
    }

    std::printf("buy participation   %.2f%%  (cap %.2f%%)\n",
                as_percent(strategy_.buy_participation_ppm()),
                as_percent(cfg_.participation_cap_ppm));
    std::printf("sell participation  %.2f%%  (cap %.2f%%)\n",
                as_percent(strategy_.sell_participation_ppm()),
                as_percent(cfg_.participation_cap_ppm));
    std::printf("quoted volume       %lu on the bid, %lu on the ask\n",
                strategy_.observed_bid_qty(), strategy_.observed_ask_qty());

    if (book_.valid) {
        std::printf("last book           bid %u @ %.4f   ask %u @ %.4f\n",
                    book_.bid_qty, as_price(book_.bid_px),
                    book_.ask_qty, as_price(book_.ask_px));
    }
}