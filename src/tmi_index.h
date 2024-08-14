// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_INDEX_H_
#define TMI_INDEX_H_

#include <functional>
#include <tuple>
#include <type_traits>

#if __cplusplus >= 202302L && defined(__cpp_static_call_operator) && __cpp_static_call_operator >= 202207L
#define TMI_CPP23_STATIC static
#define TMI_CONST_IF_NOT_CPP23_STATIC
#else
#define TMI_CPP23_STATIC
#define TMI_CONST_IF_NOT_CPP23_STATIC const
#endif

namespace tmi {
namespace detail {

struct hashed_type{};
struct ordered_type{};
struct tag_type{};

struct tag_dummy : tag_type
{
    struct empty{};
    using type = std::tuple<empty>;
};

template < typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct hashed_args
{
    static constexpr bool using_tags = std::is_base_of_v<tag_type, Arg1>;
    using tags_arg = std::conditional_t<using_tags, Arg1, tag_dummy>;
    using key_from_value_type = std::conditional_t<using_tags, Arg2, Arg1>;
    static_assert(!std::is_same_v<key_from_value_type, void>);
    using hasher_arg = std::conditional_t<using_tags, Arg3, Arg2>;
    using pred_arg = std::conditional_t<using_tags, Arg4, Arg3>;

    using default_hasher = std::hash<typename key_from_value_type::result_type>;
    using default_pred = std::equal_to<typename key_from_value_type::result_type>;

    using hasher_type = std::conditional_t<std::is_same_v<hasher_arg, void>, default_hasher, hasher_arg>;
    using pred_type = std::conditional_t<std::is_same_v<pred_arg, void>, default_pred, pred_arg>;
    using tags = typename tags_arg::type;
};

template<typename Arg1, typename Arg2, typename Arg3>
struct ordered_args
{
    static constexpr bool using_tags = std::is_base_of_v<tag_type, Arg1>;
    using tags_arg = std::conditional_t<using_tags, Arg1, tag_dummy>;
    using key_from_value_type = std::conditional_t<using_tags, Arg2, Arg1>;
    static_assert(!std::is_same_v<key_from_value_type, void>);
    using comparator_arg = std::conditional_t<using_tags, Arg3, Arg2>;

    using default_comparator = std::less<typename key_from_value_type::result_type>;

    using comparator = std::conditional_t<std::is_same_v<comparator_arg, void>, default_comparator, comparator_arg>;
    using tags = typename tags_arg::type;
};

} // namespace detail

template<typename... Tags>
struct tag : detail::tag_type
{
   using type = std::tuple<Tags...>;
};

template <typename Value>
struct identity
{
    using result_type = Value;
    TMI_CPP23_STATIC constexpr const Value& operator()(const Value& val) { return val; }
};

template < typename Arg1, typename Arg2=void, typename Arg3=void, typename Arg4=void>
struct hashed_unique : detail::hashed_type, public detail::hashed_args<Arg1, Arg2, Arg3, Arg4>
{
    static constexpr bool is_hashed_unique() { return true; }
};

template < typename Arg1, typename Arg2=void, typename Arg3=void, typename Arg4=void>
struct hashed_non_unique : detail::hashed_type, public detail::hashed_args<Arg1, Arg2, Arg3, Arg4>
{
    static constexpr bool is_hashed_unique() { return false; }
};

template<typename Arg1, typename Arg2 = void, typename Arg3 = void>
struct ordered_unique : detail::ordered_type, public detail::ordered_args<Arg1, Arg2, Arg3>
{
    static constexpr bool is_ordered_unique() { return true; }
};

template<typename Arg1, typename Arg2 = void, typename Arg3 = void>
struct ordered_non_unique : detail::ordered_type, public detail::ordered_args<Arg1, Arg2, Arg3>
{
    static constexpr bool is_ordered_unique() { return false; }
};

template<typename... Indices>
struct indexed_by
{
   using index_types = std::tuple<Indices...>;
};

} // namespace tmi

#endif // TMI_INDEX_H_
