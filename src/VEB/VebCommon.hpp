#ifndef VEBCOMMON_HPP
#define VEBCOMMON_HPP

#include <cstddef>     // std::size_t
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

#endif // VEBCOMMON_HPP
