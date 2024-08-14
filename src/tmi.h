// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_H_
#define TMI_H_

#include "tminode.h"
#include "tmi_comparator.h"
#include "tmi_hasher.h"
#include "tmi_index.h"
#include "tmi_nodehandle.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tmi {
namespace detail {

template <typename T, typename Indices, typename Allocator, typename Parent, int I>
struct index_type_helper
{
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using index_type = std::tuple_element_t<I, index_types>;
    using comparator = tmi_comparator<T, node_type, index_type, Parent, Allocator, I>;
    using hasher = tmi_hasher<T, node_type, index_type, Parent, Allocator, I>;
    using type = std::conditional_t<std::is_base_of_v<hashed_type, index_type>, hasher, comparator>;
};

} // namespace detail

template <typename T, typename Indices = indexed_by<ordered_unique<identity<T>>>, typename Allocator = std::allocator<T>>
class multi_index_container : public detail::index_type_helper<T, Indices, Allocator, multi_index_container<T, Indices, Allocator>, 0>::type
{
public:
    using parent_type = multi_index_container<T, Indices, Allocator>;
    using allocator_type = Allocator;
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using inherited_index = typename detail::index_type_helper<T, Indices, Allocator, multi_index_container<T, Indices, Allocator>, 0>::type;
    using node_handle = detail::node_handle<allocator_type, node_type>;

    static constexpr size_t num_indices = std::tuple_size<index_types>();

    template <int I>
    struct nth_index
    {
        using type = typename detail::index_type_helper<T, Indices, Allocator, multi_index_container<T, Indices, Allocator>, I>::type;
    };

    template <int I>
    using nth_index_t = typename nth_index<I>::type;

    template <typename Tag>
    struct index
    {
        template <size_t I = 0, size_t J = 0>
        static constexpr size_t get_index_for_tag()
        {
            using tags = typename std::tuple_element_t<I, index_types>::tags;
            constexpr size_t tag_count = std::tuple_size<tags>();
            using tag = typename std::tuple_element_t<J, tags>;

            if constexpr (std::is_same_v<tag, Tag>) return I;
            else if constexpr (J + 1 < tag_count) return get_index_for_tag<I, J + 1>();
            else if constexpr (I + 1 < num_indices) return get_index_for_tag<I + 1, 0>();
            else return num_indices;
        }
        static constexpr size_t value = get_index_for_tag();
        static_assert(value < num_indices, "tag not found");
        using type = nth_index_t<value>;
    };

    template <typename Tag>
    using index_t = typename index<Tag>::type;

    template <typename Tag>
    static constexpr size_t index_v = index<Tag>::value;


    template <typename Iterator>
    struct index_iterator
    {
        template <size_t I = 0>
        static constexpr size_t get_index_for_iterator()
        {
            using nth_iterator = typename nth_index_t<I>::iterator;
            if constexpr (std::is_same_v<Iterator, nth_iterator>) return I;
            else if constexpr (I + 1 < num_indices) return get_index_for_iterator<I + 1>();
            else return num_indices;
        }
        static constexpr size_t value = get_index_for_iterator();
        static_assert(value < num_indices, "iterator");
        using type = typename nth_index_t<value>::iterator;
    };

    template <typename Iterator>
    static constexpr size_t index_iterator_v = index_iterator<Iterator>::value;


    template <typename>
    struct index_tuple_helper;
    template <size_t First, size_t... ints>
    struct index_tuple_helper<std::index_sequence<First, ints...>> {
        using index_types = std::tuple<inherited_index&, nth_index_t<ints> ...>;
        using hints_types =  std::tuple<typename nth_index_t<First>::insert_hints, typename nth_index_t<ints>::insert_hints ...>;
        using ctor_args_types =  std::tuple<typename nth_index_t<First>::ctor_args, typename nth_index_t<ints>::ctor_args ...>;
        using premodify_cache_types = std::tuple<typename nth_index_t<First>::premodify_cache, typename nth_index_t<ints>::premodify_cache ...>;

        static index_types make_index_types(parent_type& parent, const allocator_type& alloc, const ctor_args_types& args) {
            return std::make_tuple(std::ref(parent), nth_index_t<ints>(parent, alloc, std::get<ints>(args)) ...);
        }
        static index_types make_index_types(parent_type& parent, const index_types& rhs) {
            return std::make_tuple(std::ref(parent), nth_index_t<ints>(parent, std::get<ints>(rhs)) ...);
        }
        static index_types make_index_types(parent_type& parent, index_types&& rhs) {
            return std::make_tuple(std::ref(parent), nth_index_t<ints>(parent, std::move(std::get<ints>(rhs))) ...);
        }
        static index_types make_index_types(parent_type& parent, const allocator_type& alloc) {
            return std::make_tuple(std::ref(parent), nth_index_t<ints>(parent, alloc) ...);
        }
    };

    using indices_tuple = typename index_tuple_helper<std::make_index_sequence<num_indices>>::index_types;
    using indices_hints_tuple = typename index_tuple_helper<std::make_index_sequence<num_indices>>::hints_types;
    using indices_premodify_cache_tuple = typename index_tuple_helper<std::make_index_sequence<num_indices>>::premodify_cache_types;
    using ctor_args_list = typename index_tuple_helper<std::make_index_sequence<num_indices>>::ctor_args_types;

    template <typename, typename, typename, typename, typename, int>
    friend class tmi_hasher;

    template <typename, typename, typename, typename, typename, int>
    friend class tmi_comparator;

private:
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};


    indices_tuple m_index_instances;

    node_allocator_type m_alloc;


    template <int I = 0, class Callable, typename Node, typename... Args>
    static void foreach_index(Callable&& func, Node node, Args&&... args)
    {
        func.template operator()<I>(node, std::get<I>(args)...);
        if constexpr (I + 1 < num_indices) {
            foreach_index<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_index(Callable&& func, Node node, Args&&... args)
    {
        if (!func.template operator()<I>(node, std::get<I>(args)...)) {
            return false;
        }
        else if constexpr (I + 1 < num_indices) {
            return get_foreach_index<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        } else {
            return true;
        }
    }

    node_type* do_insert(node_type* node)
    {
        indices_hints_tuple hints;

        bool can_insert;
        std::array<node_type*, num_indices> conflicts{};
        can_insert = get_foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, auto& hints, auto& conflict) TMI_CPP23_STATIC {
            conflict = instance.preinsert_node(node, hints);
            return conflict == nullptr;
        }, node, m_index_instances, hints, conflicts);

        if (!can_insert) {
            for (const auto& conflict : conflicts) {
                if(conflict) {
                    return conflict;
                }
            }
        }
        assert(can_insert);
        foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, const auto& hints) TMI_CPP23_STATIC {
            instance.insert_node(node, hints);
        }, node, m_index_instances,  hints);

        node->link(m_end);

        if (m_begin == nullptr) {
            assert(m_end == nullptr);
            m_begin = m_end = node;
        } else {
            m_end = node;
        }

        m_size++;
        return nullptr;
    }

    void do_erase_cleanup(node_type* node)
    {
        if (node == m_end) {
            m_end = node->prev();
        }
        if (node == m_begin) {
            m_begin = node->next();
        }
        node->unlink();
        m_size--;
    }

    void do_destroy_node(node_type* node)
    {
        std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
        std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
    }

    template <typename... Args>
    std::pair<node_type*, bool> do_emplace(Args&&... args)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, std::in_place_t{}, std::forward<Args>(args)...);
        node_type* conflict = do_insert(node);
        if (conflict != nullptr) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
            return std::make_pair(conflict, false);
        }
        return std::make_pair(node, true);
    }

    std::pair<node_type*, bool> do_insert(const T& entry)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, entry);
        node_type* conflict = do_insert(node);
        if (conflict != nullptr) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
            return std::make_pair(conflict, false);
        }
        return std::make_pair(node, true);
    }

    void do_erase(node_type* node)
    {
        foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance) TMI_CPP23_STATIC {
            instance.remove_node(node);
        }, node, m_index_instances);
        do_erase_cleanup(node);
        do_destroy_node(node);
    }

    template <typename Callable>
    bool do_modify(node_type* node, Callable&& func)
    {
        indices_premodify_cache_tuple index_cache;

        foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, auto& cache) TMI_CPP23_STATIC {
            if constexpr (nth_index_t<I>::requires_premodify_cache()) {
                instance.create_premodify_cache(node, cache);
            }
        }, node, m_index_instances,  index_cache);


        func(node->value());

        std::array<bool, num_indices> indicies_to_modify{};

        foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, auto& modify, const auto& cache) TMI_CPP23_STATIC {
            modify = instance.erase_if_modified(node, cache);
         }, node, m_index_instances,  indicies_to_modify, index_cache);


        indices_hints_tuple index_hints;

        bool insertable = get_foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, const auto& modify, auto& hints) TMI_CPP23_STATIC {
            if (modify) return instance.preinsert_node(node, hints) == nullptr;
            return true;
        }, node, m_index_instances,  indicies_to_modify, index_hints);

        if (insertable) {
            foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, const auto& modify, const auto& hints) TMI_CPP23_STATIC {
                if (modify) instance.insert_node(node, hints);
            }, node, m_index_instances,  indicies_to_modify, index_hints);
            return true;
        } else {
            foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance, const auto& modify) TMI_CPP23_STATIC {
                if (!modify) instance.remove_node(node);
            }, node, m_index_instances,  indicies_to_modify);
            do_erase_cleanup(node);
            do_destroy_node(node);
            return false;
        }
    }

    void do_clear()
    {
        foreach_index([]<int I>(std::nullptr_t, nth_index_t<I>& instance) TMI_CPP23_STATIC {
            instance.do_clear();
         }, nullptr, m_index_instances);

        auto* node = m_begin;
        while (node) {
            auto* to_delete = node;
            node = node->next();
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, to_delete);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, to_delete, 1);
        }
        m_begin = m_end = nullptr;
    }

    size_t get_size() const
    {
        return m_size;
    }

    bool get_empty() const
    {
        return m_size == 0;
    }

    node_handle do_extract(node_type* node)
    {
        if(node) {
            foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance) TMI_CPP23_STATIC {
                instance.remove_node(node);
            }, node, m_index_instances);
            do_erase_cleanup(node);
        }
        return node_handle(m_alloc, node);
    }

public:

    multi_index_container(const allocator_type& alloc = {})
        : inherited_index(*this, alloc),
          m_index_instances(index_tuple_helper<std::make_index_sequence<num_indices>>::make_index_types(*this, alloc)),
          m_alloc(alloc)
    {
    }

    multi_index_container(const ctor_args_list& args, const allocator_type& alloc = {})
        : inherited_index(*this, alloc, std::get<0>(args)),
          m_index_instances(index_tuple_helper<std::make_index_sequence<num_indices>>::make_index_types(*this, alloc, args)),
          m_alloc(alloc)
    {
    }

    ~multi_index_container()
    {
        do_clear();
    }

    multi_index_container(const multi_index_container& rhs)
        : inherited_index(*this, *static_cast<const inherited_index*>(&rhs)),
          m_index_instances(index_tuple_helper<std::make_index_sequence<num_indices>>::make_index_types(*this, rhs.m_index_instances)),
          m_alloc(std::allocator_traits<allocator_type>::select_on_container_copy_construction(rhs.m_alloc))
    {
        if (!rhs.m_size) {
            return;
        }
        node_type* from_node = rhs.m_begin;
        node_type* prev_node = nullptr;
        node_type* to_node = nullptr;
        m_begin = to_node;
        for(size_t i = 0; i < rhs.m_size; i++)
        {
            to_node = m_alloc.allocate(1);
            std::uninitialized_construct_using_allocator<node_type>(to_node, m_alloc, *from_node);
            if(i == 0) {
                m_begin = to_node;
            }
            to_node->link(prev_node);
            prev_node = to_node;
            from_node = from_node->next();
        }
        m_end = prev_node;

        to_node = m_begin;
        while(to_node) {
            foreach_index([]<int I>(node_type* node, nth_index_t<I>& instance) TMI_CPP23_STATIC {
                instance.insert_node_direct(node);
            }, to_node, m_index_instances);
            to_node = to_node->next();
            m_size++;
        }
    }

    multi_index_container(multi_index_container&& rhs)
        : inherited_index(*this, std::move(*static_cast<const inherited_index*>(&rhs))),
          m_index_instances(index_tuple_helper<std::make_index_sequence<num_indices>>::make_index_types(*this, std::move(rhs.m_index_instances))),
          m_alloc(std::move(rhs.m_alloc))
    {
        m_size = rhs.m_size;
        m_begin = rhs.m_begin;
        m_end = rhs.m_end;
        rhs.m_begin = nullptr;
        rhs.m_end = nullptr;
        rhs.m_size = 0;
    }

    static constexpr size_t node_size()
    {
        return sizeof(node_type);
    }

    template<size_t I, typename IteratorType>
    typename nth_index_t<I>::iterator project(IteratorType it)
    {
        static constexpr size_t from_iterator_index = index_iterator_v<IteratorType>;
        const node_type* node = std::get<from_iterator_index>(m_index_instances).node_from_iterator(it);
        return std::get<I>(m_index_instances).make_iterator(node);
    }

    template<size_t I, typename IteratorType>
    typename nth_index_t<I>::const_iterator project(IteratorType it) const
    {
        static constexpr size_t from_iterator_index = index_iterator_v<IteratorType>;
        const node_type* node = std::get<from_iterator_index>(m_index_instances).node_from_iterator(it);
        return std::get<I>(m_index_instances).make_iterator(node);
    }

    template<typename Tag, typename IteratorType>
    typename index_t<Tag>::iterator project(IteratorType it)
    {
        static constexpr size_t from_iterator_index = index_iterator_v<IteratorType>;
        const node_type* node = std::get<from_iterator_index>(m_index_instances).node_from_iterator(it);
        return std::get<index_v<Tag>>(m_index_instances).make_iterator(node);
    }

    template<typename Tag,typename IteratorType>
    typename index_t<Tag>::const_iterator project(IteratorType it) const
    {
        static constexpr size_t from_iterator_index = index_iterator_v<IteratorType>;
        const node_type* node = std::get<from_iterator_index>(m_index_instances).node_from_iterator(it);
        return std::get<index_v<Tag>>(m_index_instances).make_iterator(node);
    }

    template<size_t I>
    nth_index_t<I>& get() noexcept
    {
        return std::get<I>(m_index_instances);
    }

    template<int I>
    const nth_index_t<I>& get() const noexcept
    {
        return std::get<I>(m_index_instances);
    }

    template<typename Tag>
    index_t<Tag>& get() noexcept
    {
        return std::get<index_v<Tag>>(m_index_instances);
    }

    template<typename Tag>
    const index_t<Tag>& get() const noexcept
    {
        return std::get<index_v<Tag>>(m_index_instances);
    }

    allocator_type get_allocator() const noexcept
    {
        return m_alloc;
    }
};

} // namespace tmi

#endif // TMI_H_
