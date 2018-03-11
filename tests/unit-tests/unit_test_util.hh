// Utilities for unit testing
#ifndef UNIT_TEST_UTIL_HH_
#define UNIT_TEST_UTIL_HH_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include <concurrent_hash_map/concurrent_hash_map.hpp>

// Returns a statically allocated value used to keep track of how many unfreed
// bytes have been allocated. This value is shared across all threads.
std::atomic<int64_t>& get_unfreed_bytes();

// We define a a allocator class that keeps track of how many unfreed bytes have
// been allocated. Users can specify an optional bound for how many bytes can be
// unfreed, and the allocator will fail if asked to allocate above that bound
// (note that behavior with this bound with concurrent allocations will be hard
// to deal with). A bound below 0 is inactive (the default is -1).
template <class T, int64_t BOUND = -1> struct TrackingAllocator {
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template <typename U> struct rebind {
    using other = TrackingAllocator<U, BOUND>;
  };

  TrackingAllocator() {}
  template <typename U>
  TrackingAllocator(const TrackingAllocator<U, BOUND> &) {}

  T *allocate(size_t n) {
    const size_t bytes_to_allocate = sizeof(T) * n;
    if (BOUND >= 0 && get_unfreed_bytes() + bytes_to_allocate > BOUND) {
      throw std::bad_alloc();
    }
    get_unfreed_bytes() += bytes_to_allocate;
    return std::allocator<T>().allocate(n);
  }

  void deallocate(T *p, size_t n) {
    get_unfreed_bytes() -= (sizeof(T) * n);
    std::allocator<T>().deallocate(p, n);
  }

  template <typename U, class... Args> void construct(U *p, Args &&... args) {
    new ((void *)p) U(std::forward<Args>(args)...);
  }

  template <typename U> void destroy(U *p) { p->~U(); }
};

template <typename T, typename U, int64_t BOUND>
bool operator==(const TrackingAllocator<T, BOUND> &a1,
                const TrackingAllocator<U, BOUND> &a2) {
  return true;
}

template <typename T, typename U, int64_t BOUND>
bool operator!=(const TrackingAllocator<T, BOUND> &a1,
                const TrackingAllocator<U, BOUND> &a2) {
  return false;
}

using IntIntTable =
    std::concurrent_unordered_map<int, int, std::hash<int>, std::equal_to<int>,
                   std::allocator<std::pair<const int, int>>>;

template <class Alloc>
using IntIntTableWithAlloc =
    std::concurrent_unordered_map<int, int, std::hash<int>, std::equal_to<int>, Alloc>;

using StringIntTable =
    std::concurrent_unordered_map<std::string, int, std::hash<std::string>,
                   std::equal_to<std::string>,
                   std::allocator<std::pair<const std::string, int>>>;

namespace std {
template <typename T> struct hash<unique_ptr<T>> {
  size_t operator()(const unique_ptr<T> &ptr) const {
    return std::hash<T>()(*ptr);
  }

  size_t operator()(const T *ptr) const { return std::hash<T>()(*ptr); }
};

template <typename T> struct equal_to<unique_ptr<T>> {
  bool operator()(const unique_ptr<T> &ptr1, const unique_ptr<T> &ptr2) const {
    return *ptr1 == *ptr2;
  }

  bool operator()(const T *ptr1, const unique_ptr<T> &ptr2) const {
    return *ptr1 == *ptr2;
  }

  bool operator()(const unique_ptr<T> &ptr1, const T *ptr2) const {
    return *ptr1 == *ptr2;
  }
};
}

template <typename T>
using UniquePtrTable = std::concurrent_unordered_map<
    std::unique_ptr<T>, std::unique_ptr<T>, std::hash<std::unique_ptr<T>>,
    std::equal_to<std::unique_ptr<T>>,
    std::allocator<std::pair<const std::unique_ptr<T>, std::unique_ptr<T>>>>;

// Some unit tests need access into certain private data members of the table.
// This class is a friend of the table, so it can access those.

class UnitTestInternalAccess {
public:
    static const size_t IntIntBucketSize = sizeof(
        std::private_impl::bucket_container<int, int, std::allocator<std::pair<const int, int>>,
        std::private_impl::partial_t,
        std::private_impl::DEFAULT_SLOTS_PER_BUCKET>::bucket);

    /*  template <class CuckoohashMap>
        static size_t old_table_info_size(const CuckoohashMap &table) {
        // This is not thread-safe
        return table.old_table_infos.size();
        }
    */

    template <class ConcurrentHashMap>
    static typename ConcurrentHashMap::partial_t partial_key(const size_t hv) {
        return ConcurrentHashMap::partial_key(hv);
    }

    template <class ConcurrentHashMap>
    static size_t index_hash(const size_t hashpower, const size_t hv) {
        return ConcurrentHashMap::index_hash(hashpower, hv);
    }

    template <class ConcurrentHashMap>
    static size_t alt_index(const size_t hashpower,
                            const typename ConcurrentHashMap::partial_t partial,
                            const size_t index) {
        return ConcurrentHashMap::alt_index(hashpower, partial, index);
    }

    template <class ConcurrentHashMap> static size_t reserve_calc(size_t n) {
        return ConcurrentHashMap::reserve_calc(n);
    }

    template <class ConcurrentHashMap>
    static typename ConcurrentHashMap::locks_t& get_current_locks(const ConcurrentHashMap& table) {
        return table.locks;
    }

    template <class ConcurrentHashMap>
    static typename ConcurrentHashMap::size_type hashpower(const ConcurrentHashMap& table) {
        return table.hashpower();
    }
};

#endif // UNIT_TEST_UTIL_HH_
