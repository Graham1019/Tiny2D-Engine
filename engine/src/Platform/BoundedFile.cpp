#include "tiny2d/BoundedFile.h"
#include <cerrno>
#include <fcntl.h>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace tiny2d {
namespace {
struct Descriptor {
    int fd;
    ~Descriptor() { if (fd >= 0) ::close(fd); }
};
[[noreturn]] void fileError(const char* op, const std::filesystem::path& path) {
    const int err = errno;
    throw std::filesystem::filesystem_error(op, path, std::error_code(err, std::generic_category()));
}
}

std::vector<std::uint8_t> readBoundedFile(const std::filesystem::path& path, std::size_t limit) {
    Descriptor input{::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
    if (input.fd < 0) fileError("open input", path);
    struct stat info{};
    if (::fstat(input.fd, &info) < 0) fileError("stat input", path);
    if (!S_ISREG(info.st_mode))
        throw std::runtime_error("expected regular file: " + path.string());
    if (info.st_size < 0 || std::uintmax_t(info.st_size) > limit)
        throw std::runtime_error("input size " + std::to_string(info.st_size) +
                                 " exceeds byte budget " + std::to_string(limit) + ": " + path.string());
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(info.st_size));
    std::size_t pos = 0;
    while (pos < bytes.size()) {
        const auto n = ::read(input.fd, bytes.data()+pos, bytes.size()-pos);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) fileError("read input", path);
        if (!n) throw std::runtime_error("input truncated during read: " + path.string());
        pos += static_cast<std::size_t>(n);
    }
    char extra;
    ssize_t tail;
    do { tail = ::read(input.fd, &extra, 1); } while (tail < 0 && errno == EINTR);
    if (tail < 0) fileError("read tail", path);
    if (tail) throw std::runtime_error("input grew during read: " + path.string());
    return bytes;
}

} // namespace tiny2d
