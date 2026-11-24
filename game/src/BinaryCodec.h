#pragma once

#include "tiny2d_game/NavigationSnapshot.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace tiny2d_game::binary {

using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t headerBytes = 24;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24, "binary32 required");

inline std::uint32_t floatBits(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline std::uint64_t checksum(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (std::size_t i = 0; i < size; ++i)
        hash = (hash ^ data[i]) * UINT64_C(1099511628211);
    return hash;
}

class Writer {
public:
    Bytes bytes;
    void u8(std::uint8_t v) { bytes.push_back(v); }
    void u32(std::uint32_t v) {
        for (unsigned shift = 0; shift < 32; shift += 8) u8(static_cast<std::uint8_t>(v >> shift));
    }
    void u64(std::uint64_t v) {
        for (unsigned shift = 0; shift < 64; shift += 8) u8(static_cast<std::uint8_t>(v >> shift));
    }
    void f32(float v) { u32(floatBits(v)); }
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : cursor_(data), remaining_(size) {}
    explicit Reader(const Bytes& bytes) : Reader(bytes.data(), bytes.size()) {}
    std::size_t remaining() const { return remaining_; }
    const std::uint8_t* take(std::size_t n) {
        if (n > remaining_) throw std::invalid_argument("truncated binary field");
        const auto* result = cursor_;
        if (n) cursor_ += n;
        remaining_ -= n;
        return result;
    }
    void finish() const {
        if (remaining_) throw std::invalid_argument("unexpected trailing binary data");
    }
    std::uint8_t u8() { return *take(1); }
    std::uint32_t u32() {
        const auto* p = take(4);
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(p[i]) << (8*i);
        return v;
    }
    std::uint64_t u64() {
        const auto* p = take(8);
        std::uint64_t v = 0;
        for (unsigned i = 0; i < 8; ++i) v |= std::uint64_t(p[i]) << (8*i);
        return v;
    }
    float f32() {
        const auto bits = u32();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) throw std::invalid_argument("nonfinite binary32");
        return value;
    }
    bool boolean() {
        const auto v = u8();
        if (v > 1) throw std::invalid_argument("invalid boolean");
        return v != 0;
    }
private:
    const std::uint8_t* cursor_;
    std::size_t remaining_;
};

inline Bytes wrap(const char (&magic)[5], std::uint32_t version, const Bytes& payload) {
    if (payload.size() > UINT32_MAX) throw std::length_error("payload too large");
    Writer out;
    out.bytes.reserve(headerBytes + payload.size());
    out.bytes.insert(out.bytes.end(), magic, magic + 4);
    out.u32(version);
    out.u32(simulationRulesVersion);
    out.u32(static_cast<std::uint32_t>(payload.size()));
    out.u64(checksum(payload.data(), payload.size()));
    out.bytes.insert(out.bytes.end(), payload.begin(), payload.end());
    return std::move(out.bytes);
}

// The reader borrows the verified input. Its caller keeps the bytes alive.
inline Reader unwrap(const Bytes& file, const char (&magic)[5], std::uint32_t version, std::size_t limit) {
    if (file.size() > limit) throw std::length_error("file exceeds byte budget");
    Reader header(file);
    if (std::memcmp(header.take(4), magic, 4)) throw std::invalid_argument("invalid file magic");
    if (header.u32() != version) throw std::invalid_argument("unsupported format version");
    if (header.u32() != simulationRulesVersion) throw std::invalid_argument("unsupported rules version");
    const auto length = header.u32();
    const auto expected = header.u64();
    if (length != header.remaining()) throw std::invalid_argument("payload length mismatch");
    const auto* p = header.take(length);
    if (checksum(p, length) != expected) throw std::invalid_argument("payload checksum mismatch");
    return Reader(p, length);
}

} // namespace tiny2d_game::binary
