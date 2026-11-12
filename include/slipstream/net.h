//
// Created by lukeypookster on 11/12/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_NET_H
#define SLIPSTREAMEXECUTIONENGINE_NET_H

#include <cstdint>

class FileDescriptor {
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd) : fd_{fd} {}
    ~FileDescriptor();

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept;
    FileDescriptor& operator=(FileDescriptor&& other) noexcept;

    [[nodiscard]] int get() const { return fd_; }
    [[nodiscard]] int valid() const { return fd_ >= 0;; }

private:
    int fd_{-1};
};

[[nodiscard]] FileDescriptor listen_on(uint16_t port);
[[nodiscard]] FileDescriptor accept_one(const FileDescriptor& listener);

#endif //SLIPSTREAMEXECUTIONENGINE_NET_H
