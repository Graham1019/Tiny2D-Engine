#include "tiny2d_game/SnapshotCodec.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace tiny2d_game {
namespace {
struct Descriptor {
    int fd;
    ~Descriptor() { if (fd >= 0) ::close(fd); }
};
struct Temporary {
    const char* path;
    ~Temporary() { ::unlink(path); }
};
[[noreturn]] void fileError(const char* op, const std::filesystem::path& path) {
    const int err = errno;
    throw std::filesystem::filesystem_error(op, path, std::error_code(err, std::generic_category()));
}
}

void writeNewFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    auto sibling = (path.parent_path() / ".tiny2d-XXXXXX").string();
    Descriptor tmp{::mkstemp(sibling.data())};
    if (tmp.fd < 0) fileError("create sibling temporary file", path);
    Temporary cleanup{sibling.c_str()};
    std::size_t pos = 0;
    while (pos < bytes.size()) {
        const auto n = ::write(tmp.fd, bytes.data()+pos, bytes.size()-pos);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) fileError("write temporary file", path);
        if (!n) throw std::runtime_error("short write: " + path.string());
        pos += static_cast<std::size_t>(n);
    }
    if (::fsync(tmp.fd) < 0) fileError("sync temporary file", path);
    std::array<std::uint8_t, 16384> block{};
    pos = 0;
    while (pos < bytes.size()) {
        const auto length = std::min(bytes.size()-pos, block.size());
        const auto n = ::pread(tmp.fd, block.data(), length, static_cast<off_t>(pos));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) fileError("verify temporary file", path);
        if (!n || !std::equal(block.begin(), block.begin()+n, bytes.begin()+pos))
            throw std::runtime_error("temporary file verification failed: " + path.string());
        pos += static_cast<std::size_t>(n);
    }
    const int fd = tmp.fd;
    tmp.fd = -1;
    if (::close(fd) < 0) fileError("close temporary file", path);
    // Atomic publish-if-absent on the target filesystem, including symlinks.
    if (::link(sibling.c_str(), path.c_str()) < 0) fileError("publish new file", path);
}
} // namespace tiny2d_game
