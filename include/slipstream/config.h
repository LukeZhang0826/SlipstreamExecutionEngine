//
// Created by lukeypookster on 11/12/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_CONFIG_H
#define SLIPSTREAMEXECUTIONENGINE_CONFIG_H

#include <cstdint>
#include <string>

struct Config {
    std::string symbol = "SYNTH1";
    uint32_t max_quantity = 500;
    uint32_t participation_cap_ppm = 150000;
    uint64_t vwap_window_ms = 30000;
    uint32_t band_bps_x100 = 2550; // basis points (hundredth of a percent)
    std::string md_host = "127.0.0.1";
    uint16_t md_port = 14200;
    std::string oe_host = "127.0.0.1";
    uint16_t oe_port = 14300;
    std::string transport = "tcp";
};

[[nodiscard]] bool parse_args(int argc, char** argv, Config& cfg);

#endif //SLIPSTREAMEXECUTIONENGINE_CONFIG_H
