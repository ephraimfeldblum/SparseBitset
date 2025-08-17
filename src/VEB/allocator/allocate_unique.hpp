#include <concepts>    // std::same_as, std::constructible_from
#include <type_traits> // std::is_array_v, std::decay_t
#include <memory>      // std::unique_ptr, std::allocator_traits
#include <utility>     // std::forward

template <typename Allocator>
struct AllocDeleter {
  Allocator allocator;

  using pointer = typename std::allocator_traits<Allocator>::pointer;

  explicit AllocDeleter(Allocator const& a) noexcept : allocator{a} {}

  inline void operator()(pointer p) noexcept {
    using Traits = std::allocator_traits<Allocator>;
    Traits::destroy(allocator, p);
    Traits::deallocate(allocator, p, 1);
  }
};

template <typename T, typename Allocator, typename... Args,
          typename Traits = std::allocator_traits<Allocator>>
[[nodiscard]] inline auto allocate_unique(Allocator& allocator, Args&& ...args)
  -> std::unique_ptr<T, AllocDeleter<Allocator>> {
  static_assert(!std::is_array_v<T>, "Arrays not supported");
  static_assert(std::constructible_from<T, Args...>, "T is not constructible from Args");
  static_assert(std::same_as<typename Traits::value_type, T>, "Allocator's value_type must be T");
  auto p = Traits::allocate(allocator, 1);
  Traits::construct(allocator, p, std::forward<Args>(args)...);
  return std::unique_ptr<T, AllocDeleter<Allocator>>{p, AllocDeleter{allocator}};
}
