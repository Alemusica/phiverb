#pragma once

#if (__cplusplus >= 201703L || defined(__cpp_lib_optional)) && __has_include(<optional>)
#include <optional>

namespace std {
namespace experimental {
using std::optional;
using std::nullopt;
using std::nullopt_t;
using std::in_place;
using std::in_place_t;
using std::in_place_index_t;
using std::in_place_type_t;
}  // namespace experimental
}  // namespace std

#elif __has_include(<experimental/optional>)
#include <experimental/optional>

namespace std {
template <typename T>
using optional = experimental::optional<T>;

using experimental::nullopt;
using experimental::nullopt_t;
using experimental::in_place;
using experimental::in_place_t;

template <typename T>
using in_place_type_t = experimental::in_place_type_t<T>;

template <size_t I>
using in_place_index_t = experimental::in_place_index_t<I>;
}  // namespace std

#else
#error "Optional support not available on this platform."
#endif
