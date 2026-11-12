//
// Created by lukeypookster on 11/12/26.
//

#include "slipstream/net.h"

#include <cstdio>
#include <unistd.h>
#include <utility>
#include <cerrno>
#include <cstring>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>


FileDescriptor::~FileDescriptor() {
    if (fd_ >= 0) ::close(fd_);
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
    : fd_{std::exchange(other.fd_, -1)} {}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

namespace {
    void fail(const char* what) {
        std::fprintf(stderr, "%s: %s\n", what, std::strerror(errno));
    }
}

FileDescriptor listen_on(uint16_t port) {
    FileDescriptor sock{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!sock.valid()) { fail("socket"); return {}; }

    constexpr int on = 1;
    if (::setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        fail("setsockopt");
        return {};
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        fail("inet_pton");
        return {};
    }

    if (::bind(sock.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        fail("bind");
        return {};
    }

    if (::listen(sock.get(), 1) < 0) { fail("listen"); return {}; }

    return sock;
}

FileDescriptor accept_one(const FileDescriptor& listener) {
    FileDescriptor conn{::accept(listener.get(), nullptr, nullptr)};
    if (!conn.valid()) fail("accept");
    return conn;
}