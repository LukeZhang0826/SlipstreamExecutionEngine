//
// Created by lukeypookster on 11/12/26.
//

#include "slipstream/config.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

namespace {
    bool to_u64(const std::string_view s, uint64_t& out) {
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        return ec == std::errc{} && ptr == (s.data() + s.size());
    }

    bool to_scaled(const std::string_view s, const uint32_t scale, uint32_t& out) {
        const std::string text{s};
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || v < 0.0) return false;
        out = static_cast<uint32_t>(std::llround(v * scale));
        return true;
    }

    bool bad_value(const char* flag, const char* value) {
        std::fprintf(stderr, "bad value for %s: %s\n", flag, value);
        return false;
    }

}

bool parse_args(const int argc, char** argv, Config& cfg) {
    for (int i{1}; i < argc; ++i) {
        const char* flag = argv[i];
        if (i + 1 >= argc) {
            std::fprintf(stderr, "%s needs a value\n", flag);
            return false;
        }
        const char* value = argv[++i];
        const std::string_view f{flag};
        const std::string_view v{value};
        uint64_t n = 0;

        if (f == "--symbol") cfg.symbol = v;
        else if (f == "--md-host") cfg.md_host = v;
        else if (f == "--oe-host") cfg.oe_host = v;
        else if (f == "--transport") cfg.transport = v;
        else if (f == "--max-quantity") {
            if (!to_u64(v, n) || n > UINT32_MAX) return bad_value(flag, value);
            cfg.max_quantity = static_cast<uint32_t>(n);
        } else if (f == "--vwap-window-ms") {
            if (!to_u64(v, n) || n == 0) return bad_value(flag, value);
            cfg.vwap_window_ms = n;
        } else if (f == "--md-port") {
            if (!to_u64(v, n) || n > 65535) return bad_value(flag, value);
            cfg.md_port = static_cast<uint16_t>(n);
        } else if (f == "--oe-port") {
            if (!to_u64(v, n) || n > 65535) return bad_value(flag, value);
            cfg.oe_port = static_cast<uint16_t>(n);
        } else if (f == "--band-bps") {
            if (!to_scaled(v, 100, cfg.band_bps_x100)) return bad_value(flag, value);
        } else if (f == "--participation-cap") {
            if (!to_scaled(v, 1000000, cfg.participation_cap_ppm) || cfg.participation_cap_ppm > 1000000) {
                return bad_value(flag, value);
            }
        } else {
            std::fprintf(stderr, "unknown flag %s\n", flag);
            return false;
        }
    }
    return true;
}
