// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_HASHER_H_
#define TMI_HASHER_H_

#include "tmi_nodehandle.h"
#include "hash_tree.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <cstddef>
#include <iterator>
#include <limits>
#include <tuple>
#include <utility>

namespace tmi {

namespace detail {
template <typename IndexedNode, typename Hasher, typename Allocator>
struct tmi_hasher_helper
{
    using node_type = IndexedNode;
    using key_from_value = typename Hasher::key_from_value_type;
    using hasher = typename Hasher::hasher_type;
    using key_equal = typename Hasher::pred_type;
    using tree_type = hash_tree<node_type, key_from_value, hasher, key_equal, Hasher::is_hashed_unique(), Allocator>;
};
} // namespace detail

template <typename IndexedNode, typename Hasher, typename Parent, typename Allocator>
class tmi_hasher : private detail::tmi_hasher_helper<IndexedNode, Hasher, Allocator>::tree_type
{
public:
    using helper_type = detail::tmi_hasher_helper<IndexedNode, Hasher, Allocator>;
    using node_type = helper_type::node_type;
    using key_from_value = helper_type::key_from_value;
    using key_type = key_from_value::result_type;
    using hasher = helper_type::hasher;
    using key_equal = helper_type::key_equal;
    using tree_type = helper_type::tree_type;
    using size_type = size_t;
    using ctor_args = std::tuple<size_type,key_from_value,hasher,key_equal>;
    using allocator_type = Allocator;
    using node_handle = detail::node_handle<Allocator, node_type>;
    using iterator = tree_type::iterator;
    using const_iterator = tree_type::const_iterator;
    using insert_return_type = detail::insert_return_type<iterator, node_handle>;
    using value_type = node_type::value_type;
    friend Parent;

private:

    using insert_hints = tree_type::insert_hints;
    using premodify_cache = tree_type::premodify_cache;
    static constexpr bool requires_premodify_cache() { return tree_type::requires_premodify_cache(); }

    Parent& m_parent;

    tmi_hasher(Parent& parent, const allocator_type& alloc) : tree_type{alloc}, m_parent(parent) {}

    tmi_hasher(Parent& parent, const allocator_type& alloc, const ctor_args& args) : tree_type(alloc, std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args)), m_parent(parent){}

    tmi_hasher(Parent& parent, const tmi_hasher& rhs) : tree_type(static_cast<tree_type&>(rhs)), m_parent(parent){}
    tmi_hasher(Parent& parent, tmi_hasher&& rhs) : tree_type(std::move(static_cast<tree_type&>(rhs))), m_parent(parent)
    {
    }

    void tmi_remove_node(const node_type* node)
    {
        iterator it = tree_type::make_iterator(node);
        tree_type::erase(it);
    }

    void tmi_insert_node_direct(node_type* node)
    {
        tree_type::insert_node_direct(node);
    }

    node_type* tmi_preinsert_node(const node_type* node, insert_hints& hints)
    {
        return tree_type::preinsert_node(node, hints);
    }

    void tmi_create_premodify_cache(const node_type* node, premodify_cache& cache)
    {
        tree_type::create_premodify_cache(node, cache);
    }

    bool tmi_erase_if_modified(const node_type* node, const premodify_cache& cache)
    {
        return tree_type::erase_if_modified(node, cache);
    }

    void tmi_insert_node(node_type* node, const insert_hints& hints)
    {
        tree_type::insert_node(node, hints);
    }

    void tmi_clear()
    {
        tree_type::clear();
    }

public:

    iterator begin()
    {
        return tree_type::begin();
    }

    const_iterator begin() const
    {
        return tree_type::begin();
    }

    iterator end()
    {
        return tree_type::end();
    }

    const_iterator end() const
    {
        return tree_type::end();
    }

    const_iterator iterator_to(const value_type& entry) const
    {
        const node_type* node = Parent::template value_cast<node_type>(entry);
        return tree_type::make_iterator(node);
    }

    iterator iterator_to(const value_type& entry)
    {
        node_type* node = Parent::template value_cast<node_type>(const_cast<value_type&>(entry));
        return tree_type::make_iterator(node);
    }

    template <typename... Args>
    std::pair<iterator,bool> emplace(Args&&... args)
    {
        auto [node, success] = m_parent.template do_emplace<node_type>(std::forward<Args>(args)...);
        return std::make_pair(tree_type::make_iterator(node), success);
    }

    std::pair<iterator,bool> insert(const value_type& value)
    {
        auto [node, success] = m_parent.template do_insert<node_type>(value);
        return std::make_pair(tree_type::make_iterator(node), success);
    }

    std::pair<iterator,bool> insert(value_type&& value)
    {
        auto [node, success] = m_parent.template do_insert<node_type>(std::move(value));
        return std::make_pair(tree_type::make_iterator(node), success);
    }

    template <typename Callable>
    bool modify(iterator it, Callable&& func)
    {
        node_type* node = const_cast<node_type*>(tree_type::node_from_iterator(it));
        if (!node) return false;
        return m_parent.do_modify(node, std::forward<Callable>(func));
    }

    template <typename CompatibleKey>
    iterator find(const CompatibleKey& key) const
    {
        return tree_type::find(key);
    }

    iterator erase(iterator it)
    {
        node_type* node = const_cast<node_type*>(tree_type::node_from_iterator(it));
        ++it;
        m_parent.do_erase(node);
        return it;
    }

    template <typename CompatibleKey>
    size_t count(const CompatibleKey& key) const
    {
        return tree_type::count(key);
    }

    void clear()
    {
        m_parent.do_clear();
    }

    size_t size() const
    {
        return tree_type::size();
    }

    bool empty() const
    {
        return tree_type::empty();
    }

    insert_return_type insert(node_handle&& handle)
    {
        node_type* node = handle.m_node;
        if(!node) {
            return {end(), false, {}};
        }
        auto ret = m_parent.do_insert(node);
        const auto& [new_node, inserted] = ret;
        if (!inserted) {
            return {tree_type::make_iterator(new_node), false, std::move(handle)};
        }
        handle.m_node = nullptr;
        return {tree_type::make_iterator(new_node), true, {}};
    }

    const_iterator make_iterator(const node_type* node) const
    {
        return tree_type::make_iterator(node);
    }

    node_handle extract(const_iterator it)
    {
        const node_type* node = tree_type::node_from_iterator(it);
        return m_parent.do_extract(const_cast<node_type*>(node));
    }

    allocator_type get_allocator() const noexcept
    {
        return m_parent.get_allocator();
    }
};

} // namespace tmi

#endif // TMI_HASHER_H_
