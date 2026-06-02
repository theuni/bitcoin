// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_COMPARATOR_H_
#define TMI_COMPARATOR_H_

#include "tmi_nodehandle.h"
#include "wavl_tree.h"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <tuple>

namespace tmi {

namespace detail {
template <typename IndexedNode, typename Comparator>
struct tmi_comparator_helper
{
    using node_type = IndexedNode;
    using key_from_value = typename Comparator::key_from_value_type;
    using key_compare = typename Comparator::comparator;
    using tree_type = wavl_tree<node_type, key_from_value, key_compare, Comparator::is_ordered_unique()>;
};
} // namespace detail

template <typename IndexedNode, typename Comparator, typename Parent, typename Allocator>
class tmi_comparator : private detail::tmi_comparator_helper<IndexedNode, Comparator>::tree_type
{
public:

    using helper_type = detail::tmi_comparator_helper<IndexedNode, Comparator>;
    using node_type = helper_type::node_type;
    using key_from_value = helper_type::key_from_value;
    using tree_type = helper_type::tree_type;
    using key_compare = typename Comparator::comparator;
    using key_type = typename key_from_value::result_type;
    using ctor_args = std::tuple<key_from_value,key_compare>;
    using allocator_type = Allocator;
    using node_handle = detail::node_handle<Allocator, node_type>;
    using iterator = tree_type::iterator;
    using const_iterator = tree_type::const_iterator;
    using insert_return_type = detail::insert_return_type<iterator, node_handle>;
    using value_type = node_type::value_type;

private:
    friend Parent;

    using insert_hints = tree_type::insert_hints;
    using premodify_cache = tree_type::premodify_cache;
    static constexpr bool requires_premodify_cache() { return tree_type::requires_premodify_cache(); }

    Parent& m_parent;

    tmi_comparator(Parent& parent, const allocator_type&) : m_parent(parent){}

    tmi_comparator(Parent& parent, const allocator_type&, const ctor_args& args) : tree_type{std::get<0>(args), std::get<1>(args)}, m_parent(parent){}
    tmi_comparator(Parent& parent, const tmi_comparator& rhs) : tree_type{static_cast<tree_type&>(rhs)}, m_parent(parent){}
    tmi_comparator(Parent& parent, tmi_comparator&& rhs) : tree_type{static_cast<tree_type&>(std::move(rhs))}, m_parent(parent){}

    tmi_comparator(const tree_type&) = delete;
    tmi_comparator(tree_type&&) = delete;
    tmi_comparator& operator=(const tree_type&) = delete;
    tmi_comparator& operator=(tree_type&&) = delete;

    void tmi_remove_node(node_type* node)
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

    void tmi_insert_node(node_type* node, const insert_hints& hints)
    {
        tree_type::insert_node(node, hints);
    }

    bool tmi_erase_if_modified(node_type* node, const premodify_cache& cache)
    {
        return tree_type::erase_if_modified(node, cache);
    }

    void tmi_clear()
    {
        tree_type::clear();
    }

public:

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

    iterator begin() const
    {
        return tree_type::begin();
    }

    iterator end() const
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

    template <typename Callable>
    bool modify(iterator it, Callable&& func)
    {
        node_type* node = const_cast<node_type*>(tree_type::node_from_iterator(it));
        if (!node) return false;
        return m_parent.do_modify(node, std::forward<Callable>(func));
    }

    template<typename CompatibleKey>
    iterator find(const CompatibleKey& key) const
    {
        return tree_type::find(key);
    }

    template<typename CompatibleKey>
    iterator lower_bound(const CompatibleKey& key) const
    {
        return tree_type::lower_bound(key);
    }

    template<typename CompatibleKey>
    iterator upper_bound(const CompatibleKey& key) const
    {
        return tree_type::upper_bound(key);
    }

    template<typename CompatibleKey>
    size_t count(const CompatibleKey& key) const
    {
        return tree_type::count(key);
    }

    iterator erase(iterator it)
    {
        node_type* node = const_cast<node_type*>(tree_type::node_from_iterator(it));
        ++it;
        m_parent.do_erase(node);
        return it;
    }

    size_t erase(const key_type& key) const
    {
        size_t ret{0};
        auto [first, last] = tree_type::equal_range(key);
        for(auto it = first; it != last;)
        {
            it = erase(it);
            ret++;
        }
        return ret;
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
            return {tree_type::end(), false, {}};
        }
        auto ret = m_parent.do_insert(node);
        const auto& [new_node, inserted] = ret;
        if (!inserted) {
            return {tree_type::make_iterator(new_node), false, std::move(handle)};
        }
        handle.m_node = nullptr;
        return {tree_type::make_iterator(new_node), true, {}};
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

    const_iterator make_iterator(const node_type* node) const
    {
        return tree_type::make_iterator(node);
    }

};

} // namespace tmi

#endif // TMI_COMPARATOR_H_
