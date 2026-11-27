//
// Created by lukeypookster on 11/21/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_ENGINE_H
#define SLIPSTREAMEXECUTIONENGINE_ENGINE_H

#include <cstddef>
#include <string>
#include <string_view>

#include "book.h"
#include "config.h"
#include "frame_reader.h"
#include "messages.h"
#include "net.h"
#include "vwap.h"
#include "codec.h"
#include "strategy.h"

class Engine {
public:
    struct Stats {
        size_t quotes_received = 0; // quotes for any symbol
        size_t quotes_kept = 0; // for the config symbol
        size_t trades_received = 0;
        size_t trades_kept = 0;
        size_t accepted = 0;
        size_t rejected = 0;
        size_t halted = 0;
        size_t suppressed = 0; // trades arrived before warmup
        size_t reject_reasons[kRejectReasonCount] = {};
        uint64_t offered_qty = 0;
    };

    explicit Engine(const Config& cfg);

    [[nodiscard]] bool bind();

    void run();

    void report() const;

    void on_quote(const Quote& q);
    void on_trade(const Trade& t);

    [[nodiscard]] const Stats& stats() const { return stats_; }
    [[nodiscard]] const L1& book() const { return book_; }
    [[nodiscard]] const RollingVwap& vwap() const { return vwap_; }
    [[nodiscard]] const Strategy& strategy() const { return strategy_; }

private:
    enum class LinkState { Listening, Connected, Closed };
    enum class SessionState { Open, Halted };

    void accept_md();
    void accept_oe();
    void service_md();
    void service_oe();

    void end_session();
    void check_oe_staleness();

    void service_stdin();
    void handle_command(std::string_view line);
    void announce_session(uint8_t state);

    [[nodiscard]] int poll_timeout_ms() const;

    [[nodiscard]] bool send_oe(const OeMessage& msg);

    void send_verdict(const Trade& t, const Decision& d);

    [[nodiscard]] bool done() const {
        return md_ == LinkState::Closed && oe_ == LinkState::Closed;
    }

    Config       cfg_;
    Stats        stats_;
    SymbolFilter filter_;
    RollingVwap  vwap_;
    L1           book_;
    Strategy     strategy_;

    uint64_t     next_order_id_ = 0;
    uint64_t     oe_idle_since_ns_ = 0;

    SessionState session_ = SessionState::Open;
    bool stdin_open_ = true;
    std::string stdin_line_;

    FileDescriptor md_listener_;
    FileDescriptor oe_listener_;
    FileDescriptor md_conn_;
    FileDescriptor oe_conn_;

    LinkState md_ = LinkState::Listening;
    LinkState oe_ = LinkState::Listening;

    FrameReader<MdMessage> md_reader_;
    FrameReader<OeMessage> oe_reader_;
};

#endif //SLIPSTREAMEXECUTIONENGINE_ENGINE_H
