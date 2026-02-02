#ifndef VEBCOMMON_HPP
#define VEBCOMMON_HPP

#include <cstddef>     // std::size_t
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits> // std::remove_cvref_t

struct VebTreeMemoryStats {
    std::size_t total_clusters{};
    std::size_t max_depth{};
    std::size_t total_nodes{};
};

template<typename S>
using index_t = typename std::remove_cvref_t<S>::index_t;

template<typename... Fs>
struct overload : Fs... {
    using Fs::operator()...;
};

template<typename... Fs>
overload(Fs...) -> overload<Fs...>;

enum struct VebSerializeTag : std::uint8_t {
    NODE0  = 0,
    NODE8  = 1,
    NODE16 = 2,
    NODE32 = 3,
    NODE64 = 4,
};

template <typename S>
constexpr auto tag_v = [] {
    if constexpr (std::is_same_v<std::remove_cvref_t<S>, std::monostate>) {
        return VebSerializeTag::NODE0;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<S>, struct Node8>) {
        return VebSerializeTag::NODE8;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<S>, struct Node16>) {
        return VebSerializeTag::NODE16;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<S>, struct Node32>) {
        return VebSerializeTag::NODE32;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<S>, struct Node64>) {
        return VebSerializeTag::NODE64;
    } else {
        static_assert(sizeof(S) == 0, "Unsupported type for tag_v");
    }
}();

// serialization helpers (little-endian)
inline void write_u8(std::string &out, std::uint8_t v) {
    out.push_back(static_cast<char>(v));
}

inline void write_tag(std::string &out, VebSerializeTag tag) {
    write_u8(out, static_cast<std::uint8_t>(tag));
}

inline void write_u16(std::string &out, std::uint16_t v) {
    for (auto i{0uz}; i < 2; ++i) {
        write_u8(out, static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

inline void write_u32(std::string &out, std::uint32_t v) {
    for (auto i{0uz}; i < 4; ++i) {
        write_u8(out, static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

inline void write_u64(std::string &out, std::uint64_t v) {
    for (auto i{0uz}; i < 8; ++i) {
        write_u8(out, static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

inline std::uint8_t read_u8(std::string_view buf, std::size_t &pos) {
    if (pos + 1 > buf.size()) {
        throw std::runtime_error("buffer too small for u8");
    }
    return static_cast<std::uint8_t>(buf[pos++]);
}

inline VebSerializeTag read_tag(std::string_view buf, std::size_t &pos) {
    return static_cast<VebSerializeTag>(read_u8(buf, pos));
}

inline std::uint16_t read_u16(std::string_view buf, std::size_t &pos) {
    if (pos + 2 > buf.size()) {
        throw std::runtime_error("buffer too small for u16");
    }
    std::uint16_t v = 0;
    for (auto i{0uz}; i < 2; ++i) {
        v |= static_cast<std::uint16_t>(static_cast<std::uint16_t>(read_u8(buf, pos)) << (8 * i));
    }
    return v;
}

inline std::uint32_t read_u32(std::string_view buf, std::size_t &pos) {
    if (pos + 4 > buf.size()) {
        throw std::runtime_error("buffer too small for u32");
    }
    std::uint32_t v = 0;
    for (auto i{0uz}; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(read_u8(buf, pos)) << (8 * i);
    }
    return v;
}

inline std::uint64_t read_u64(std::string_view buf, std::size_t &pos) {
    if (pos + 8 > buf.size()) {
        throw std::runtime_error("buffer too small for u64");
    }
    std::uint64_t v = 0;
    for (auto i{0uz}; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(read_u8(buf, pos)) << (8 * i);
    }
    return v;
}

#endif // VEBCOMMON_HPP
