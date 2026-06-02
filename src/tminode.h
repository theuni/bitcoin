#ifndef TMINODE_H
#define TMINODE_H

#include "tmi_index.h"

#include <array>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cassert>

namespace tmi {

template<typename T>
concept inheritable = !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_fundamental_v<T>;

template <class T>
concept not_inheritable = !inheritable<T>;

template <typename T, typename Indices>
class tminode;

template <typename T>
struct Holder;

template <inheritable T>
struct Holder<T> : protected T
{
    template <typename... Args>
    Holder(Args&&... args) : T(std::forward<Args>(args)...){}

    Holder(const Holder&) = delete;
    Holder(Holder&&) = delete;
    Holder operator=(Holder&&) = delete;
    Holder operator=(const Holder&) = delete;

    constexpr const T& get() const { return *this; }
    constexpr T& get() { return *this; }

};

template <not_inheritable T>
struct Holder<T>
{
    T m_val;
    constexpr const T& get() const { return m_val; }
    constexpr T& get() { return m_val; }
    template <typename... Args>
    Holder(Args&&... args) : m_val(std::forward<Args>(args)...){}

    Holder(const Holder&) = delete;
    Holder(Holder&&) = delete;
    Holder operator=(Holder&&) = delete;
    Holder operator=(const Holder&) = delete;
};

template <typename T, typename Indices, int I>
class tmi_indexed_node;

template <typename T, typename Indices, int I>
class tmi_indexed_comparator_node;

template <typename T, typename Indices, int I>
class tmi_indexed_hash_node;

template <typename T, typename Indices>
class tminode : public Holder<T>
{
protected:
    tminode* m_prev{nullptr};
    tminode* m_next{nullptr};
    struct rb {
        tminode* m_left{nullptr};
        tminode* m_right{nullptr};
        uintptr_t par_and_flg{};
    };
    struct hash_type {
        tminode* m_nexthash{nullptr};
        size_t m_hash{0};
    };

    template <int I>
    using comparator_indexed_node_type = tmi_indexed_comparator_node<T, Indices, I>;

    template <int I>
    using hash_indexed_node_type = tmi_indexed_hash_node<T, Indices, I>;

    using index_types = typename Indices::index_types;
    template <int I>
    struct base_index_type_helper
    {
        using index_type = std::tuple_element_t<I, index_types>;
        using data_type = std::conditional_t<std::is_base_of_v<detail::hashed_type, index_type>, hash_type, rb>;
        using indexed_node_type = std::conditional_t<std::is_base_of_v<detail::hashed_type, index_type>, hash_indexed_node_type<I>, comparator_indexed_node_type<I>>;
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
    template <typename, typename, int>
    friend class tmi_indexed_comparator_node;

    template <typename, typename, int>
    friend class tmi_indexed_hash_node;

    template <int I>
    using indexed_node_type = base_index_type_helper<I>::indexed_node_type;

    using value_type = T;

    template <typename... Args>
    tminode(Args&&... args) : Holder<T>(std::forward<Args>(args)...)
    {
    }

    const T& value() const { return Holder<T>::get(); }
    T& value() { return Holder<T>::get(); }

    tminode* next() const { return m_next; }
    tminode* prev() const { return m_prev; }

    void link(tminode* prev)
    {
        if (prev) {
            prev->m_next = this;
        }
        m_prev = prev;
    }

    void unlink()
    {
        if (m_prev)
            m_prev->m_next = m_next;
        if (m_next)
            m_next->m_prev = m_prev;
        m_prev = nullptr;
        m_next = nullptr;
    }
};

template <typename T, typename Indices, int I>
class tmi_indexed_hash_node : private tminode<T, Indices>
{
    tmi_indexed_hash_node() = delete;
public:
    using typename tminode<T, Indices>::value_type;
    using tminode<T, Indices>::value;

    tmi_indexed_hash_node* next_hash() const
    {
        return static_cast<tmi_indexed_hash_node*>(std::get<I>(tminode<T, Indices>::m_data).m_nexthash);
    }

    size_t hash() const
    {
        return std::get<I>(tminode<T, Indices>::m_data).m_hash;
    }

    void set_hash(size_t hash)
    {
        std::get<I>(tminode<T, Indices>::m_data).m_hash = hash;
    }

    void set_next_hashptr(tmi_indexed_hash_node* rhs)
    {
        std::get<I>(tminode<T, Indices>::m_data).m_nexthash = rhs;
    }
};

template <typename T, typename Indices, int I>
class tmi_indexed_comparator_node : private tminode<T, Indices>
{
    tmi_indexed_comparator_node() = delete;
public:

    using typename tminode<T, Indices>::value_type;
    using tminode<T, Indices>::value;

    tmi_indexed_comparator_node* parent() const
    {
        return reinterpret_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).par_and_flg & ~1UL);
    }

    void set_parent(tmi_indexed_comparator_node *p)
    {
        auto& par_and_flg = std::get<I>(tminode<T, Indices>::m_data).par_and_flg;
        par_and_flg = (par_and_flg & 1UL) | reinterpret_cast<uintptr_t>(p);
    }

    uintptr_t flags() const
    {
        return std::get<I>(tminode<T, Indices>::m_data).par_and_flg & 1UL;
    }

    bool rp() const
    {
        return std::get<I>(tminode<T, Indices>::m_data).par_and_flg & 1UL;
    }

    void flip()
    {
        std::get<I>(tminode<T, Indices>::m_data).par_and_flg ^= 1UL;
    }

    void clr_flags()
    {
        std::get<I>(tminode<T, Indices>::m_data).par_and_flg &= ~1UL;
    }

    tmi_indexed_comparator_node* left() const
    {
        return static_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).m_left);
    }

    tmi_indexed_comparator_node* right() const
    {
        return static_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).m_right);
    }

    void set_right(tmi_indexed_comparator_node* node)
    {
        std::get<I>(tminode<T, Indices>::m_data).m_right = node;
    }

    void set_left(tmi_indexed_comparator_node* node)
    {
        std::get<I>(tminode<T, Indices>::m_data).m_left = node;
    }

    void set_ch(int d, tmi_indexed_comparator_node* rhs)
    {
        if (d) {
            std::get<I>(tminode<T, Indices>::m_data).m_right = rhs;
        } else {
            std::get<I>(tminode<T, Indices>::m_data).m_left = rhs;
        }
    }

    tmi_indexed_comparator_node* ch(int d)
    {
        return d ? static_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).m_right)
          :  static_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).m_left);
    }

    const tmi_indexed_comparator_node* ch(int d) const
    {
        return d ? static_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).m_right)
          :  static_cast<tmi_indexed_comparator_node*>(std::get<I>(tminode<T, Indices>::m_data).m_left);
    }

    uintptr_t par_and_flg() const
    {
        return std::get<I>(tminode<T, Indices>::m_data).par_and_flg;
    }

    void set_par_and_flg(uintptr_t rhs)
    {
        std::get<I>(tminode<T, Indices>::m_data).par_and_flg = rhs;
    }
};


} // namespace tmi
#endif // TMINODE_H
