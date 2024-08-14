#ifndef TMINODE_BASE_H
#define TMINODE_BASE_H

#include "tmi_index.h"

#include <array>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tmi {

template <typename T, typename Indices>
class tminode;

template <typename T, typename Indices>
class tminode_base {
    struct rb {
        tminode_base* m_left{nullptr};
        tminode_base* m_right{nullptr};
        tminode_base* m_parent{nullptr};
    };
    struct hash {
        tminode_base* m_nexthash{nullptr};
        size_t m_hash{0};
    };

    /* Pointer back to self. This is a hack which enables the tminode_base
       structure to be instantiated as required by the red-black-tree
       implementation. See note towards the top of manysortfind.h for
       more tminode_base.

       libc++ uses inheritance to create a base class which contains only
       pointer/color tminode_base. That doesn't work here as we require T to be the
       first member to enable casting T* to node*. See note in
       manysortfind::iterator_to.

       Ideally this will be removed once the libc++ red-black-tree
       functions have been replaced.

    */
    tminode<T, Indices>* m_node{nullptr};

    using index_types = typename Indices::index_types;
    template <int I>
    struct base_index_type_helper
    {
        using index_type = std::tuple_element_t<I, index_types>;
        using data_type = std::conditional_t<std::is_base_of_v<detail::hashed_type, index_type>, hash, rb>;
    };

    template <typename>
    struct base_index_helper;
    template <size_t... ints>
    struct base_index_helper<std::index_sequence<ints...>> {
        using data_types = std::tuple<typename base_index_type_helper<ints>::data_type ...>;
    };
    static constexpr size_t num_indices = std::tuple_size<index_types>();
    using data_types_tuple = typename base_index_helper<std::make_index_sequence<num_indices>>::data_types;

    data_types_tuple m_data;

public:
    friend class tminode<T, Indices>;
    enum Color : bool {
        RED = false,
        BLACK = true
    };

    template <int I>
    void set_right(tminode_base* rhs)
    {
        std::get<I>(m_data).m_right = rhs;
    }

    template <int I>
    void set_left(tminode_base* rhs)
    {
        std::get<I>(m_data).m_left = rhs;
    }


    /* The following functions use Boost's pointer compression trick to
       encode the red/black bit in the parent pointer. It makes the
       assumption that no sane compiler will ever allow this pointer to
       be set to an odd memory address. */

    template <int I>
    tminode_base* parent() const
    {
        static constexpr uintptr_t mask = std::numeric_limits<uintptr_t>::max() - 1;
        auto addr = reinterpret_cast<uintptr_t>(std::get<I>(m_data).m_parent) & mask;
        return reinterpret_cast<tminode_base*>(addr);
    }

    template <int I>
    void set_parent(tminode_base* rhs)
    {
        static constexpr uintptr_t mask = 1;
        auto prev = reinterpret_cast<uintptr_t>(std::get<I>(m_data).m_parent) & mask;
        auto newaddr = reinterpret_cast<uintptr_t>(rhs) | prev;
        std::get<I>(m_data).m_parent = reinterpret_cast<tminode_base*>(newaddr);
    }

    template <int I>
    Color color() const
    {
        static constexpr uintptr_t mask = 1;
        return (reinterpret_cast<uintptr_t>(std::get<I>(m_data).m_parent) & mask) == 0 ? Color::RED : Color::BLACK;
    }

    template <int I>
    void set_color(Color rhs)
    {
        static constexpr uintptr_t mask = std::numeric_limits<uintptr_t>::max() - 1;
        auto addr = reinterpret_cast<uintptr_t>(std::get<I>(m_data).m_parent) & mask;
        std::get<I>(m_data).m_parent = reinterpret_cast<tminode_base*>(addr | static_cast<uintptr_t>(rhs));
    }

    template <int I>
    tminode_base* left() const
    {
        return std::get<I>(m_data).m_left;
    }

    template <int I>
    tminode_base* right() const
    {
        return std::get<I>(m_data).m_right;
    }


    tminode<T, Indices>* node() const
    {
        return m_node;
    }

    template <int I>
    tminode_base* next_hash() const
    {
        return std::get<I>(m_data).m_nexthash;
    }

    template <int I>
    size_t hash() const
    {
        return std::get<I>(m_data).m_hash;
    }

    template <int I>
    void set_hash(size_t hash)
    {
        std::get<I>(m_data).m_hash = hash;
    }

    template <int I>
    void set_next_hashptr(tminode_base* rhs)
    {
        std::get<I>(m_data).m_nexthash = rhs;
    }
};

} // namespace tmi
#endif // TMINODE_BASE_H
