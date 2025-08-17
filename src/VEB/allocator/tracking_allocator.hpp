/**
 * @file tracking_allocator.hpp
 * @brief A custom allocator that tracks memory usage
 *
 * This allocator wraps around std::malloc and maintains a running count
 * of allocated and deallocated bytes through a reference to an external counter.
 */

#ifndef TRACKING_ALLOCATOR_HPP
#define TRACKING_ALLOCATOR_HPP

#include <memory>
#include <cstddef>
#include <cstdlib>
#include <limits>

/**
 * @brief A tracking allocator that monitors memory usage
 * 
 * This allocator maintains a reference to an external std::size_t counter
 * that tracks the total number of bytes currently allocated. The counter
 * is incremented on allocation and decremented on deallocation.
 * 
 * @tparam T The type of objects to allocate
 */
template<typename T>
class tracking_allocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    template<typename U>
    struct rebind {
        using other = tracking_allocator<U>;
    };

private:
    std::size_t& bytes_allocated_;

public:
    /**
     * @brief Constructs a tracking allocator
     * @param bytes_allocated Reference to the counter that tracks allocated bytes
     */
    explicit tracking_allocator(std::size_t& bytes_allocated) noexcept
        : bytes_allocated_(bytes_allocated) {}

    /**
     * @brief Copy constructor
     */
    tracking_allocator(const tracking_allocator& other) noexcept
        : bytes_allocated_(other.bytes_allocated_) {}

    /**
     * @brief Converting copy constructor
     */
    template<typename U>
    tracking_allocator(const tracking_allocator<U>& other) noexcept
        : bytes_allocated_(other.get_counter()) {}

    /**
     * @brief Assignment operator
     */
    tracking_allocator& operator=([[maybe_unused]] const tracking_allocator& other) noexcept {
        // Note: We don't assign bytes_allocated_ since it's a reference
        // and should refer to the same counter throughout the allocator's lifetime
        return *this;
    }

    /**
     * @brief Destructor
     */
    ~tracking_allocator() = default;

    /**
     * @brief Allocates memory for n objects of type T
     * @param n Number of objects to allocate space for
     * @return Pointer to the allocated memory, or nullptr if allocation fails
     */
    pointer allocate(size_type n) noexcept {
        if (n == 0) return nullptr;

        // Check for overflow
        if (n > max_size()) {
            return nullptr;
        }

        const size_type bytes = n * sizeof(T);
        void* ptr = std::malloc(bytes);

        if (!ptr) {
            return nullptr;
        }

        bytes_allocated_ += bytes;

        return static_cast<pointer>(ptr);
    }

    /**
     * @brief Deallocates memory
     * @param ptr Pointer to the memory to deallocate
     * @param n Number of objects that were allocated
     */
    void deallocate(pointer ptr, size_type n) noexcept {
        if (ptr == nullptr || n == 0) return;

        const size_type bytes = n * sizeof(T);
        bytes_allocated_ -= bytes;

        std::free(ptr);
    }

    /**
     * @brief Constructs an object at the given location
     */
    template<typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        std::construct_at(ptr, std::forward<Args>(args)...);
    }

    /**
     * @brief Destroys an object at the given location
     */
    template<typename U>
    void destroy(U* ptr) {
        std::destroy_at(ptr);
    }

    /**
     * @brief Returns the maximum number of objects that can be allocated
     */
    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    /**
     * @brief Gets a reference to the byte counter (for rebind)
     */
    std::size_t& get_counter() const noexcept {
        return bytes_allocated_;
    }

    /**
     * @brief Equality comparison
     */
    template<typename U>
    bool operator==(const tracking_allocator<U>& other) const noexcept {
        return &bytes_allocated_ == &other.get_counter();
    }

    /**
     * @brief Inequality comparison
     */
    template<typename U>
    bool operator!=(const tracking_allocator<U>& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Helper function to create a tracking allocator
 * @param bytes_allocated Reference to the counter
 * @return A tracking allocator for type T
 */
template<typename T>
tracking_allocator<T> make_tracking_allocator(std::size_t& bytes_allocated) {
    return tracking_allocator<T>(bytes_allocated);
}

#endif // TRACKING_ALLOCATOR_HPP
