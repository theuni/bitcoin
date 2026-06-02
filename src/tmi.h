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
    using indexed_node_type = typename node_type::template indexed_node_type<I>;
    using comparator = tmi_comparator<indexed_node_type, index_type, Parent, Allocator>;
    using hasher = tmi_hasher<indexed_node_type, index_type, Parent, Allocator>;
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

    template <int I>
    using indexed_node_type = node_type::template indexed_node_type<I>;

    template <typename IndexedNode>
    using node_handle = detail::node_handle<allocator_type, IndexedNode>;

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

    template <typename, typename, typename, typename>
    friend class tmi_hasher;

    template <typename, typename, typename, typename>
    friend class tmi_comparator;

private:
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};


    indices_tuple m_index_instances;

    [[no_unique_address]] node_allocator_type m_alloc;

    template <int I = 0, class Callable, typename... Args>
    static void foreach_index(Callable&& func, std::nullptr_t, Args&&... args)
    {
        func.template operator()<I>(std::get<I>(args)...);
        if constexpr (I + 1 < num_indices) {
            foreach_index<I + 1>(std::forward<Callable>(func), nullptr, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static void foreach_index(Callable&& func, Node node, Args&&... args)
    {
        func.template operator()<I>(indexed_node_cast<indexed_node_type<I>>(node), std::get<I>(args)...);
        if constexpr (I + 1 < num_indices) {
            foreach_index<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_index(Callable&& func, Node node, Args&&... args)
    {
        if (!func.template operator()<I>(indexed_node_cast<indexed_node_type<I>>(node), std::get<I>(args)...)) {
            return false;
        }
        else if constexpr (I + 1 < num_indices) {
            return get_foreach_index<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        } else {
            return true;
        }
    }

    template <typename IndexedNode>
    std::pair<IndexedNode*, bool> do_insert(IndexedNode* indexed)
    {
        indices_hints_tuple hints;
        
        node_type* node = node_cast(indexed);
        node_type* conflict = nullptr;
        get_foreach_index([&conflict]<int I>(const indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, auto& indexed_hints) TMI_CPP23_STATIC {
            auto* ret = instance.tmi_preinsert_node(indexed_node, indexed_hints);
            if (ret) {
                conflict = node_cast(ret);
                return false;
            }
            return true;
        }, node, m_index_instances, hints);

        if(conflict) {
            return {indexed_node_cast<IndexedNode>(conflict), false};
        }
        foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, const auto& indexed_hints) TMI_CPP23_STATIC {
            instance.tmi_insert_node(indexed_node, indexed_hints);
        }, node, m_index_instances,  hints);

        node->link(m_end);

        if (m_begin == nullptr) {
            assert(m_end == nullptr);
            m_begin = m_end = node;
        } else {
            m_end = node;
        }

        m_size++;
        return {indexed, true};
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

    template <typename IndexedNode, typename... Args>
    std::pair<IndexedNode*, bool> do_emplace(Args&&... args)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, std::forward<Args>(args)...);
        auto ret = do_insert(indexed_node_cast<IndexedNode>(node));
        const auto& [it, inserted] = ret;
        if (!inserted) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
        }
        return ret;
    }

    template <typename IndexedNode>
    std::pair<IndexedNode*, bool> do_insert(const T& entry)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, entry);
        auto ret = do_insert(indexed_node_cast<IndexedNode>(node));
        const auto& [it, inserted] = ret;
        if (!inserted) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
        }
        return ret;
    }

    template <typename IndexedNode>
    std::pair<IndexedNode*, bool> do_insert(T&& entry)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, std::move(entry));
        auto ret = do_insert(indexed_node_cast<IndexedNode>(node));
        const auto& [it, inserted] = ret;
        if (!inserted) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
        }
        return ret;
    }

    template <typename IndexedNode>
    void do_erase(IndexedNode* indexed)
    {
        node_type* node = node_cast(indexed);
        foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance) TMI_CPP23_STATIC {
            instance.tmi_remove_node(indexed_node);
        }, node, m_index_instances);
        do_erase_cleanup(node);
        do_destroy_node(node);
    }

    template <typename IndexedNode, typename Callable>
    bool do_modify(IndexedNode* indexed, Callable&& func)
    {
        node_type* node = node_cast(indexed);
        indices_premodify_cache_tuple index_cache;

        foreach_index([]<int I>(const indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, auto& cache) TMI_CPP23_STATIC {
            if constexpr (nth_index_t<I>::requires_premodify_cache()) {
                instance.tmi_create_premodify_cache(indexed_node, cache);
            }
        }, node, m_index_instances,  index_cache);


        func(node->value());

        std::array<bool, num_indices> indicies_to_modify{};

        foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, auto& modify, const auto& cache) TMI_CPP23_STATIC {
            modify = instance.tmi_erase_if_modified(indexed_node, cache);
         }, node, m_index_instances,  indicies_to_modify, index_cache);


        indices_hints_tuple index_hints;

        bool insertable = get_foreach_index([]<int I>(const indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, const auto& modify, auto& indexed_hints) TMI_CPP23_STATIC {
            if (modify) return instance.tmi_preinsert_node(indexed_node, indexed_hints) == nullptr;
            return true;
        }, node, m_index_instances,  indicies_to_modify, index_hints);

        if (insertable) {
            foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, const auto& modify, const auto& indexed_hints) TMI_CPP23_STATIC {
                if (modify) instance.tmi_insert_node(indexed_node, indexed_hints);
            }, node, m_index_instances,  indicies_to_modify, index_hints);
            return true;
        } else {
            foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance, const auto& modify) TMI_CPP23_STATIC {
                if (!modify) instance.tmi_remove_node(indexed_node);
            }, node, m_index_instances,  indicies_to_modify);
            do_erase_cleanup(node);
            do_destroy_node(node);
            return false;
        }
    }

    void do_clear()
    {
        foreach_index([]<int I>(nth_index_t<I>& instance) TMI_CPP23_STATIC {
            instance.tmi_clear();
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

    template <typename IndexedNode>
    node_handle<IndexedNode> do_extract(IndexedNode* indexed)
    {
        if(!indexed) {
            return {};
        }
        node_type* node = node_cast(indexed);
        foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance) TMI_CPP23_STATIC {
             instance.tmi_remove_node(indexed_node);
         }, node, m_index_instances);
        do_erase_cleanup(node);
        return {m_alloc, indexed};
    }

    template <typename IndexedNode>
    static constexpr IndexedNode* indexed_node_cast(node_type* node)
    {
        static_assert(std::is_base_of_v<node_type, IndexedNode>);
        static_assert(sizeof(IndexedNode) - sizeof(node_type) == 0);
        return reinterpret_cast<IndexedNode*>(node);
    }

    template <typename IndexedNode>
    static constexpr const IndexedNode* indexed_node_cast(const node_type* node)
    {
        static_assert(std::is_base_of_v<node_type, IndexedNode>);
        static_assert(sizeof(IndexedNode) - sizeof(node_type) == 0);
        return reinterpret_cast<const IndexedNode*>(node);
    }

    template <typename IndexedNode>
    static constexpr node_type* node_cast(IndexedNode* node)
    {
        static_assert(std::is_base_of_v<node_type, IndexedNode>);
        static_assert(sizeof(IndexedNode) - sizeof(node_type) == 0);
        return reinterpret_cast<node_type*>(node);
    }

    template <typename IndexedNode>
    static constexpr const node_type* node_cast(const IndexedNode* node)
    {
        static_assert(std::is_base_of_v<node_type, IndexedNode>);
        static_assert(sizeof(IndexedNode) - sizeof(node_type) == 0);
        return reinterpret_cast<const node_type*>(node);
    }

    template <typename IndexedNode>
    static constexpr IndexedNode* value_cast(T& elem)
    {
#if defined __has_builtin
#   if __has_builtin (__builtin_offsetof)
        if constexpr(not_inheritable<T>) {
#           pragma GCC diagnostic push
#           pragma GCC diagnostic ignored "-Winvalid-offsetof"
            static_assert(__builtin_offsetof(node_type, m_val) == 0);
#           pragma GCC diagnostic pop
        }
#   endif
#endif
        return indexed_node_cast<IndexedNode>(reinterpret_cast<node_type*>(&elem));
    }

    template <typename IndexedNode>
    static constexpr const IndexedNode* value_cast(const T& elem)
    {
#if defined __has_builtin
#   if __has_builtin (__builtin_offsetof)
        if constexpr(not_inheritable<T>) {
#           pragma GCC diagnostic push
#           pragma GCC diagnostic ignored "-Winvalid-offsetof"
            static_assert(__builtin_offsetof(node_type, m_val) == 0);
#           pragma GCC diagnostic pop
        }
#   endif
#endif
        return indexed_node_cast<const IndexedNode>(reinterpret_cast<const node_type*>(&elem));
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
            foreach_index([]<int I>(indexed_node_type<I>* indexed_node, nth_index_t<I>& instance) TMI_CPP23_STATIC {
                instance.tmi_insert_node_direct(indexed_node);
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

    template <int I, typename IteratorType>
    class IteratorProject
    {
        static constexpr size_t from_iterator_index = index_iterator_v<IteratorType>;
        using from_const_iterator_type = nth_index_t<from_iterator_index>::const_iterator;
        using from_iterator_type = nth_index_t<from_iterator_index>::iterator;
        using to_const_iterator_type = typename nth_index_t<I>::const_iterator;
        using to_iterator_type = typename nth_index_t<I>::iterator;

        static_assert(std::is_same_v<IteratorType, from_const_iterator_type> || std::is_same_v<IteratorType, from_iterator_type>);

        public:
        using projected_iterator_type = std::conditional_t<std::is_same_v<IteratorType, from_const_iterator_type>, to_const_iterator_type, to_iterator_type>;

        static projected_iterator_type convert(const indices_tuple& index_instances, IteratorType it)
        {
            auto* indexed_node = std::get<from_iterator_index>(index_instances).node_from_iterator(it);
            auto* node = node_cast(indexed_node);
            return std::get<I>(index_instances).make_iterator(indexed_node_cast<indexed_node_type<I>>(node));
        }
    };

    template<size_t I, typename IteratorType>
    typename IteratorProject<I, IteratorType>::projected_iterator_type project(IteratorType it)
    {
        return IteratorProject<I, IteratorType>::convert(m_index_instances, it);
    }

    template<size_t I, typename IteratorType>
    typename IteratorProject<I, IteratorType>::projected_iterator_type project(IteratorType it) const
    {
        return IteratorProject<I, IteratorType>::convert(m_index_instances, it);
    }

    template<typename Tag, typename IteratorType>
    typename IteratorProject<index_v<Tag>, IteratorType>::projected_iterator_type project(IteratorType it)
    {
        return IteratorProject<index_v<Tag>, IteratorType>::convert(m_index_instances, it);
    }

    template<typename Tag,typename IteratorType>
    typename IteratorProject<index_v<Tag>, IteratorType>::projected_iterator_type project(IteratorType it) const
    {
        return IteratorProject<index_v<Tag>, IteratorType>::convert(m_index_instances, it);
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
